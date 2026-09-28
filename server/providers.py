from piper import PiperVoice
import random
import audioop, base64, json, os, subprocess, time, uuid, wave
from urllib import request as http
from urllib.error import HTTPError

class GroqSTT:
    def transcribe(self,path):
        started=time.monotonic(); boundary='----CursoreBoundary'+uuid.uuid4().hex
        pre=(f'--{boundary}\r\nContent-Disposition: form-data; name="model"\r\n\r\nwhisper-large-v3-turbo\r\n--{boundary}\r\nContent-Disposition: form-data; name="language"\r\n\r\nit\r\n--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="request.wav"\r\nContent-Type: audio/wav\r\n\r\n').encode()
        with open(path,'rb') as f: body=pre+f.read()+f'\r\n--{boundary}--\r\n'.encode()
        req=http.Request('https://api.groq.com/openai/v1/audio/transcriptions',data=body,headers={'Authorization':'Bearer '+os.environ['GROQ_API_KEY'],'Content-Type':f'multipart/form-data; boundary={boundary}','User-Agent':'cursore-server/1.0'})
        with http.urlopen(req,timeout=20) as r: result=json.load(r)
        return {'text':result['text'].strip(),'ms':round((time.monotonic()-started)*1000)}

class GroqLLM:
    def complete(self, text):
        started = time.monotonic()
        model = os.getenv('GROQ_LLM_MODEL', 'openai/gpt-oss-20b')

        payload = {
            'model': model,
            'messages': [
                {
                    'role': 'system',
                    'content': (
                        'Sei Cursore, un assistente vocale italiano. '
                        'Rispondi direttamente in italiano, in modo naturale, '
                        'corretto e molto conciso. Usa normalmente una o due frasi. '
                        'La risposta sarà pronunciata da un altoparlante. '
                        'Non usare markdown, URL, elenchi o citazioni. '
                        'Se non disponi di informazioni aggiornate necessarie '
                        'per rispondere con affidabilità, dichiaralo chiaramente.'
                    )
                },
                {
                    'role': 'user',
                    'content': text
                }
            ],
            'reasoning_effort': 'low',
            'max_completion_tokens': 200
        }

        req = http.Request(
            'https://api.groq.com/openai/v1/chat/completions',
            data=json.dumps(payload).encode(),
            headers={
                'Authorization': 'Bearer ' + os.environ['GROQ_API_KEY'],
                'Content-Type': 'application/json',
                'User-Agent': 'cursore-server/1.0'
            }
        )

        try:
            with http.urlopen(req, timeout=15) as r:
                result = json.load(r)

            answer = result['choices'][0]['message']['content'].strip()

            if not answer:
                raise ValueError('empty_llm_response')

            elapsed = round((time.monotonic() - started) * 1000)

            print(
                f'llm_ok model={model} ms={elapsed}',
                flush=True
            )

            return {
                'text': answer,
                'ms': elapsed,
                'provider': model
            }

        except Exception as exc:
            print(
                f'llm_failed model={model} '
                f'type={type(exc).__name__} error={str(exc)[:160]}',
                flush=True
            )
            raise


class PiperTTS:
    _voice = None
    _model_name = None
    _load_ms = None

    @classmethod
    def _get_voice(cls):
        model = os.getenv(
            'PIPER_MODEL',
            'it_IT-riccardo-x_low'
        )

        if cls._voice is not None and cls._model_name == model:
            return cls._voice

        model_path = f'/voices/{model}.onnx'
        config_path = f'/voices/{model}.onnx.json'

        started = time.monotonic()

        cls._voice = PiperVoice.load(
            model_path,
            config_path=config_path
        )

        cls._model_name = model
        cls._load_ms = round(
            (time.monotonic() - started) * 1000
        )

        print(
            f'piper_loaded model={model} '
            f'load_ms={cls._load_ms}',
            flush=True
        )

        return cls._voice

    def synthesize(self, text, output):
        voice = self._get_voice()

        started = time.monotonic()
        first_chunk_ms = None
        chunk_count = 0
        audio_bytes = 0

        wav_file = None

        try:
            for chunk in voice.synthesize(text):

                if first_chunk_ms is None:
                    first_chunk_ms = round(
                        (time.monotonic() - started) * 1000
                    )

                    wav_file = wave.open(output, 'wb')
                    wav_file.setframerate(chunk.sample_rate)
                    wav_file.setsampwidth(chunk.sample_width)
                    wav_file.setnchannels(chunk.sample_channels)

                pcm = chunk.audio_int16_bytes

                wav_file.writeframes(pcm)

                audio_bytes += len(pcm)
                chunk_count += 1

            if wav_file is None:
                raise ValueError('piper_produced_no_audio')

        finally:
            if wav_file is not None:
                wav_file.close()

        synthesis_ms = round(
            (time.monotonic() - started) * 1000
        )

        print(
            f'piper_ok model={self._model_name} '
            f'first_chunk_ms={first_chunk_ms} '
            f'synthesis_ms={synthesis_ms} '
            f'chunks={chunk_count} '
            f'bytes={audio_bytes}',
            flush=True
        )

        return {
            'ms': synthesis_ms,
            'conversion_ms': 0,
            'provider': 'piper-' + self._model_name,
            'first_chunk_ms': first_chunk_ms
        }


class GeminiTTS:
    def synthesize(self,text,output):
        started=time.monotonic(); model=os.getenv('GEMINI_TTS_MODEL','gemini-3.1-flash-tts-preview')
        payload={'contents':[{'parts':[{'text':'Parla in italiano con tono naturale e amichevole da assistente personale. '+text}]}], 'generationConfig':{'responseModalities':['AUDIO'],'speechConfig':{'voiceConfig':{'prebuiltVoiceConfig':{'voiceName':os.getenv('GEMINI_TTS_VOICE','Kore')}},'languageCode':'it-IT'}}}
        url=f'https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent?key={os.environ["GEMINI_API_KEY"]}'
        req=http.Request(url,data=json.dumps(payload).encode(),headers={'Content-Type':'application/json','User-Agent':'cursore-server/1.0'})
        with http.urlopen(req,timeout=30) as r: result=json.load(r)
        inline=result['candidates'][0]['content']['parts'][0]['inlineData']; raw=base64.b64decode(inline['data'])
        rate=int(inline['mimeType'].split('rate=')[1].split(';')[0]); channels=int(inline['mimeType'].split('channels=')[1])
        conversion_started=time.monotonic()
        if channels != 1: raw,_=audioop.tomono(raw,2,0.5,0.5); channels=1
        if rate != 16000: raw,_=audioop.ratecv(raw,2,channels,rate,16000,None); rate=16000
        with wave.open(output,'wb') as w: w.setnchannels(1); w.setsampwidth(2); w.setframerate(16000); w.writeframes(raw)
        return {'ms':round((time.monotonic()-started)*1000),'conversion_ms':round((time.monotonic()-conversion_started)*1000),'provider':'gemini-3.1-flash-tts-preview'}
