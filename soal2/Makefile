CC      = gcc
CFLAGS  = -Wall -pthread
LDFLAGS = -lrt -lpthread

all: orion eternal

orion: orion.c arena.h
	$(CC) $(CFLAGS) orion.c -o orion $(LDFLAGS)

eternal: eternal.c arena.h
	$(CC) $(CFLAGS) eternal.c -o eternal $(LDFLAGS)

clean:
	rm -f orion eternal eterion_save.dat

# Hapus semua IPC resource
clear_ipc:
	@echo "=== Membersihkan IPC ==="
	-ipcrm -M 0x00001234 2>/dev/null && echo "  [OK] Shared Memory" || echo "  [--] Shared Memory tidak ada"
	-ipcrm -Q 0x00005678 2>/dev/null && echo "  [OK] Message Queue" || echo "  [--] Message Queue tidak ada"
	-ipcrm -S 0x00009012 2>/dev/null && echo "  [OK] Semaphore"     || echo "  [--] Semaphore tidak ada"
	@echo "================================"

# Reset total: IPC + binary + save
reset: clear_ipc clean
	@echo "Full reset selesai. Jalankan 'make' untuk rebuild."

.PHONY: all clean clear_ipc reset