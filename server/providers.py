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

class GeminiLLM:
    def complete(self, text):
        started = time.monotonic()
        model = os.getenv('GEMINI_MODEL', 'gemini-3.6-flash')

        prompt = (
            'Sei Cursore, un assistente vocale italiano. '
            'Rispondi direttamente alla domanda in italiano. '
            'Usa una o due frasi brevi, naturali e complete. '
            'La risposta sarà pronunciata da un altoparlante. '
            'Non usare markdown, URL, elenchi o citazioni. '
            'Se la domanda richiede dati aggiornati che non puoi verificare, '
            'dillo chiaramente invece di inventare una risposta. '
            'Richiesta: ' + text
        )

        payload = {
            'contents': [
                {'parts': [{'text': prompt}]}
            ],
            'generationConfig': {
                'temperature': 0.3,
                'maxOutputTokens': 512,
                'thinkingConfig': {
                    'thinkingLevel': 'MINIMAL'
                }
            }
        }

        url = (
            'https://generativelanguage.googleapis.com/v1beta/'
            f'models/{model}:generateContent'
            f'?key={os.environ["GEMINI_API_KEY"]}'
        )

        delays = [0, 1, 2]
        last_error = None

        for attempt, delay in enumerate(delays, 1):
            if delay:
                time.sleep(delay)

            try:
                req = http.Request(
                    url,
                    data=json.dumps(payload).encode(),
                    headers={
                        'Content-Type': 'application/json',
                        'User-Agent': 'cursore-server/1.0'
                    }
                )

                with http.urlopen(req, timeout=35) as r:
                    result = json.load(r)

                parts = result['candidates'][0]['content']['parts']

                answer = ''.join(
                    part.get('text', '')
                    for part in parts
                    if isinstance(part, dict)
                    and not part.get('thought', False)
                ).strip()

                if not answer:
                    raise ValueError('empty_llm_response')

                elapsed = round(
                    (time.monotonic() - started) * 1000
                )

                print(
                    f'llm_ok model={model} '
                    f'attempt={attempt} ms={elapsed}',
                    flush=True
                )

                return {
                    'text': answer,
                    'ms': elapsed
                }

            except HTTPError as exc:
                last_error = exc

                print(
                    f'llm_retry model={model} '
                    f'attempt={attempt} status={exc.code}',
                    flush=True
                )

                if exc.code not in (429, 500, 502, 503, 504):
                    raise

            except TimeoutError as exc:
                last_error = exc

                print(
                    f'llm_retry model={model} '
                    f'attempt={attempt} timeout',
                    flush=True
                )

        raise last_error

class PiperTTS:
    def synthesize(self,text,output):
        started=time.monotonic(); command=['python','-m','piper','--data-dir','/voices','-m',os.getenv('PIPER_MODEL','it_IT-paola-medium'),'-f',output,'--',text]
        subprocess.run(command,check=True,timeout=20,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
        return {'ms':round((time.monotonic()-started)*1000),'conversion_ms':0,'provider':'piper-riccardo-x_low'}

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
