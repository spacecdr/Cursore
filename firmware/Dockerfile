FROM espressif/idf:v6.0.3

WORKDIR /project
COPY CMakeLists.txt IDF_VERSION partitions.csv sdkconfig.defaults ./
COPY main/ ./main/

# The public build must work without local Wi-Fi credentials.
RUN cp main/secrets.example.h main/secrets.h

CMD ["idf.py", "build"]
