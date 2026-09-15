import json, os, re, shutil, tempfile, time, uuid, wave, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from providers import GroqSTT, GeminiLLM, PiperTTS, GeminiTTS

HOST=os.getenv('CURSORE_HOST','0.0.0.0'); PORT=int(os.getenv('CURSORE_PORT','8766'))
MAX_SECONDS=int(os.getenv('CURSORE_MAX_SECONDS','30')); MAX_BYTES=16000*2*MAX_SECONDS
ROOT='/tmp/cursore'; stored={}; lock=threading.Lock()

def reply(h, code, value):
    body=json.dumps(value,ensure_ascii=False).encode(); h.send_response(code)
    h.send_header('Content-Type','application/json; charset=utf-8'); h.send_header('Content-Length',str(len(body))); h.end_headers(); h.wfile.write(body)

class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt,*args): print(fmt%args,flush=True)
    def read_pcm(self, out):
        if self.headers.get('Transfer-Encoding','').lower() != 'chunked':
            remaining=int(self.headers.get('Content-Length','0'))
            if remaining > MAX_BYTES: raise ValueError('audio_too_large')
            total=0
            while remaining:
                chunk=self.rfile.read(min(8192,remaining))
                if not chunk: break
                out.write(chunk); total+=len(chunk); remaining-=len(chunk)
            return total
        total=0
        while True:
            line=self.rfile.readline(32)
            if not line: raise ValueError('incomplete_chunked_body')
            try: size=int(line.strip().split(b';',1)[0],16)
            except ValueError: raise ValueError('invalid_chunk_size')
            if size==0:
                while self.rfile.readline(8192) not in (b'\r\n',b'\n',b''): pass
                return total
            if total+size > MAX_BYTES: raise ValueError('audio_too_large')
            remaining=size
            while remaining:
                chunk=self.rfile.read(min(8192,remaining))
                if not chunk: raise ValueError('incomplete_chunked_body')
                out.write(chunk); total+=len(chunk); remaining-=len(chunk)
            if self.rfile.read(2) != b'\r\n': raise ValueError('invalid_chunk_ending')
    def do_GET(self):
        m=re.fullmatch(r'/api/v1/requests/([0-9a-fA-F-]{8,64})/audio',self.path)
        if self.path=='/health': return reply(self,200,{'status':'ok','service':'cursore-server','api_version':'v1'})
        if not m: return reply(self,404,{'status':'not_found'})
        item=stored.get(m.group(1)); path=item and item['audio']
        if not path or not os.path.isfile(path): return reply(self,404,{'status':'not_found','error':'audio_not_found'})
        self.send_response(200); self.send_header('Content-Type','audio/wav'); self.send_header('Content-Length',str(os.path.getsize(path))); self.send_header('X-Request-Id',m.group(1)); self.end_headers()
        with open(path,'rb') as f: shutil.copyfileobj(f,self.wfile,8192)
    def do_POST(self):
        if self.path!='/api/v1/requests': return reply(self,404,{'status':'not_found'})
        rid=self.headers.get('X-Request-Id') or str(uuid.uuid4()); device=self.headers.get('X-Device-Id','unknown')
        if not re.fullmatch(r'[0-9a-fA-F-]{8,64}',rid): return reply(self,400,{'status':'error','error':'invalid_request_id'})
        required={'X-Audio-Sample-Rate':'16000','X-Audio-Channels':'1','X-Audio-Encoding':'pcm_s16le'}
        if any(self.headers.get(k)!=v for k,v in required.items()): return reply(self,415,{'status':'error','error':'invalid_audio_headers','required':required})
        if not lock.acquire(False): return reply(self,429,{'status':'error','error':'request_in_progress'})
        started=time.monotonic(); directory=tempfile.mkdtemp(prefix=rid+'-',dir=ROOT); pcm=os.path.join(directory,'request.pcm'); wavfile=os.path.join(directory,'request.wav'); audio=os.path.join(directory,'response.wav')
        try:
            with open(pcm,'wb') as out:
                total=self.read_pcm(out)
            if total<2 or total>MAX_BYTES or total%2: raise ValueError('invalid_audio_size')
            with wave.open(wavfile,'wb') as out:
                out.setnchannels(1); out.setsampwidth(2); out.setframerate(16000)
                with open(pcm,'rb') as source: out.writeframes(source.read())
            stt=GroqSTT().transcribe(wavfile); llm=GeminiLLM().complete(stt['text'])
            try:
                tts=(GeminiTTS() if os.getenv('TTS_PROVIDER','gemini')=='gemini' else PiperTTS()).synthesize(llm['text'],audio)
            except Exception as tts_error:
                print(f'tts_primary_failed fallback=piper error={type(tts_error).__name__}',flush=True)
                tts=PiperTTS().synthesize(llm['text'],audio)
            total_ms=round((time.monotonic()-started)*1000); stored[rid]={'audio':audio,'created':time.time()}
            threading.Timer(300, lambda: (stored.pop(rid, None), shutil.rmtree(directory, ignore_errors=True))).start()
            print(f'request={rid} device={device} bytes={total} total_ms={total_ms}',flush=True)
            return reply(self,200,{'request_id':rid,'status':'completed','transcript':stt['text'],'response_text':llm['text'],'audio':{'url':f'/api/v1/requests/{rid}/audio','content_type':'audio/wav','sample_rate':16000,'channels':1,'encoding':'pcm_s16le','size_bytes':os.path.getsize(audio)},'timings_ms':{'stt':stt['ms'],'llm':llm['ms'],'tts':tts['ms'],'audio_conversion':tts['conversion_ms'],'total':total_ms},'providers':{'stt':'groq-whisper-large-v3-turbo','llm':'gemini-3.6-flash','tts':tts['provider']}})
        except Exception as exc:
            print(
                f'request={rid} failed type={type(exc).__name__} error={str(exc)[:160]}',
                flush=True
            )
            return reply(self,400 if isinstance(exc,ValueError) else 502,{'request_id':rid,'status':'error','error':str(exc)[:160]})
        finally:
            for path in (pcm, wavfile):
                try: os.unlink(path)
                except FileNotFoundError: pass
            lock.release()

os.makedirs(ROOT,exist_ok=True); server=ThreadingHTTPServer((HOST,PORT),Handler); print(f'cursore-server listening on {HOST}:{PORT}',flush=True); server.serve_forever()
