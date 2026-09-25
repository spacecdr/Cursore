FROM espressif/idf:v6.0.3

WORKDIR /project
COPY . .

# The public build must work without local Wi-Fi credentials.
RUN test ! -f main/secrets.h || rm -f main/secrets.h
RUN cp main/secrets.example.h main/secrets.h

CMD ["idf.py", "build"]
