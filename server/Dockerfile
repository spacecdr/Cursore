FROM python:3.11-slim

WORKDIR /app
COPY requirements.txt ./requirements.txt
RUN pip install --no-cache-dir -r requirements.txt \
    && python -m piper.download_voices it_IT-paola-medium it_IT-riccardo-x_low --download-dir /voices
COPY index.py providers.py ./

ENV PIPER_DATA_DIR=/voices
USER nobody
EXPOSE 8766
CMD ["python", "index.py"]
