FROM gcc:latest
WORKDIR /app

# Copiamo i file mantenendo le loro cartelle originali
COPY server/server.c ./server/
COPY shared/shared.h ./shared/

# Compiliamo chiamando l'eseguibile "server_app" per non confonderlo con la cartella
RUN gcc server/server.c -o server_app -lpthread

# Avviamo il nuovo eseguibile
CMD ["./server_app"]