# The Wired — Aplikasi Chat Berbasis Terminal

> *"No matter where you go, everyone's connected."*

The Wired adalah aplikasi chat sederhana berbasis terminal yang berjalan di atas jaringan lokal. Terinspirasi dari serial anime **Serial Experiments Lain**, program ini memungkinkan beberapa pengguna terhubung ke satu server dan saling berkirim pesan secara real-time. Ada juga fitur admin tersembunyi untuk mengawasi dan mengelola server.

---

## Struktur File

```
.
├── protocol.h   # Definisi bersama (konstanta, tipe data, struktur paket)
├── navi.c       # Program server (The Wired)
└── wired.c      # Program client (pengguna yang terhubung)
```

---

## Cara Kerja

### `protocol.h` — Bahasa Bersama

File ini adalah "perjanjian" antara server dan client. Di sini didefinisikan:

- **Port server**: `5000`
- **Struktur `Packet`**: format data yang dikirim bolak-balik, berisi tipe pesan, nama pengirim, isi pesan, dan perintah RPC.
- **Jenis pesan (`MsgType`)**: chat biasa, pesan sistem, permintaan/respons RPC, autentikasi, dan perintah keluar.
- **Perintah admin (`RpcCommand`)**: lihat daftar pengguna, cek uptime server, dan shutdown darurat.

---

### `navi.c` — Server

Server adalah jantung dari The Wired. Begitu dijalankan, ia akan mendengarkan koneksi masuk di port `5000` dan menangani setiap pengguna di *thread* terpisah.

**Alur kerja server:**

1. **Menerima koneksi** baru dari client.
2. **Registrasi nama**: client mengirim nama yang diinginkan. Jika nama sudah dipakai orang lain, server menolak dan meminta nama lain.
3. **Cek admin**: kalau nama yang didaftarkan adalah `"The Knights"`, server akan meminta password. Kalau benar (`lembuBerkah`), pengguna mendapat hak admin. Kalau salah, langsung diputus.
4. **Loop pesan**: setelah terdaftar, server meneruskan pesan chat ke semua pengguna lain (*broadcast*).
5. **Perintah admin (RPC)**: admin bisa meminta daftar user aktif, uptime server, atau mematikan server secara darurat.
6. **Cleanup**: ketika pengguna keluar atau koneksi putus, server memberi tahu semua pengguna lain.

Semua aktivitas (koneksi, pesan, shutdown) dicatat ke file `history.log`.

---

### `wired.c` — Client

Program yang dijalankan oleh pengguna untuk terhubung ke The Wired.

**Alur kerja client:**

1. **Koneksi** ke server di `127.0.0.1:5000`.
2. **Registrasi**: pengguna memasukkan nama. Kalau sudah dipakai, diminta coba lagi.
3. **Mode biasa**: setelah terdaftar, pengguna bisa langsung mengetik pesan. Ketik `/exit` untuk keluar.
4. **Mode admin** (khusus `"The Knights"`): setelah autentikasi berhasil, tampil konsol khusus dengan 4 pilihan:
   - `1` — Lihat daftar pengguna aktif
   - `2` — Lihat uptime server
   - `3` — Jalankan shutdown darurat
   - `4` — Disconnect

Client menggunakan *thread* terpisah untuk menerima pesan, sehingga pesan masuk bisa muncul di layar tanpa mengganggu proses mengetik.

---

## Cara Menjalankan

### Kompilasi

```bash
gcc navi.c -o navi -lpthread
gcc wired.c -o wired -lpthread
```

### Jalankan Server

```bash
./navi
```

Server akan aktif dan mulai menunggu koneksi. Tekan `Ctrl+C` untuk mematikan server secara normal.

### Jalankan Client

```bash
./wired
```

Buka terminal baru untuk setiap pengguna yang ingin terhubung.

---

## Fitur Admin

Untuk masuk sebagai admin, gunakan nama **`The Knights`** saat registrasi. Server akan meminta password:

```
Password: lembuBerkah
```

Setelah berhasil, akan muncul konsol admin:

```
=== THE KNIGHTS CONSOLE ===
1. Check Active Entities (Users)
2. Check Server Uptime
3. Execute Emergency Shutdown
4. Disconnect
```

> ⚠️ **Peringatan**: Pilihan 3 akan mematikan server secara paksa dan memutus semua pengguna yang sedang terhubung.

---

## Catatan Teknis

- Server mendukung maksimal **64 pengguna** secara bersamaan.
- Setiap pengguna ditangani oleh *thread* tersendiri menggunakan `pthread`.
- Akses ke data bersama dijaga oleh `mutex` untuk menghindari *race condition*.
- Semua log tersimpan di file **`history.log`** dalam format `[YYYY-MM-DD HH:MM:SS] [aktor] [status]`.
- Server menggunakan `select()` agar bisa mendeteksi sinyal shutdown darurat dari admin tanpa memblokir proses penerimaan koneksi baru.

# ⚔️ Battle of Eterion — Dokumentasi Lengkap

---

## Gambaran Umum

Battle of Eterion adalah game pertempuran berbasis terminal yang menggunakan
**Inter-Process Communication (IPC)** Linux. Terdiri dari dua program:

- `orion`  — Server / Arena: menerima perintah, mengelola state game
- `eternal` — Client / Prajurit: tampilan UI, input pemain, komunikasi ke server

Keduanya berkomunikasi lewat tiga mekanisme IPC:
- **Shared Memory** : data real-time (HP, status battle, challenge)
- **Message Queue** : pengiriman perintah client → server dan response balik
- **Semaphore**     : mutex untuk mencegah race condition di shared memory

---

## Struktur File

```
soal2/
├── arena.h           ← definisi struct, konstanta, helper inline
├── orion.c           ← server (1 proses, multi-thread untuk bot)
├── eternal.c         ← client (1 proses per pemain, thread render)
├── Makefile          ← build, clean, reset IPC
└── eterion_save.dat  ← file persistensi data pemain (dibuat otomatis)
```

---

## Cara Build & Menjalankan

```bash
# 1. Build
make

# 2. Terminal 1 — jalankan server DULU
./orion

# 3. Terminal 2 — jalankan client pemain pertama
./eternal

# 4. Terminal 3 (opsional) — client pemain kedua untuk PvP
./eternal
```

---

## Cara Reset

```bash
# Hapus IPC saja (jika server crash tanpa cleanup)
make clear_ipc

# Reset total: IPC + binary + save data
make reset
make        # build ulang
```

Perintah `make clear_ipc` menjalankan:
```
ipcrm -M 0x00001234   ← hapus Shared Memory
ipcrm -Q 0x00005678   ← hapus Message Queue
ipcrm -S 0x00009012   ← hapus Semaphore
```

---

## arena.h — Header Bersama

File ini di-include oleh KEDUA program. Berisi semua definisi yang
dipakai bersama antara server dan client.

### Include System Headers
```c
#include <stdio.h>      // printf, fopen, fread, fwrite
#include <stdlib.h>     // malloc, free, exit
#include <string.h>     // memset, strncpy, strcmp
#include <unistd.h>     // sleep, usleep, read, getpid
#include <pthread.h>    // pthread_create, pthread_join
#include <sys/ipc.h>    // IPC_CREAT, IPC_RMID
#include <sys/shm.h>    // shmget, shmat, shmdt, shmctl
#include <sys/msg.h>    // msgget, msgsnd, msgrcv, msgctl
#include <sys/sem.h>    // semget, semop, semctl
#include <time.h>       // time, difftime, strftime
#include <signal.h>     // signal, SIGINT, SIGTERM
#include <termios.h>    // tcgetattr, tcsetattr (raw mode terminal)
#include <fcntl.h>      // fcntl (non-blocking fd)
```

### IPC Keys
```c
#define SHM_KEY  0x00001234   // kunci untuk shmget()
#define MSG_KEY  0x00005678   // kunci untuk msgget()
#define SEM_KEY  0x00009012   // kunci untuk semget()
```
Key adalah angka unik yang dipakai oleh shmget/msgget/semget agar
server dan client bisa mengakses resource IPC yang SAMA di kernel.

### Konstanta Game
```c
#define MAX_PLAYERS     16    // maks pemain terdaftar di shared memory
#define MAX_NAME        32    // panjang maks username/password
#define MAX_PASS        32
#define MAX_HISTORY     20    // maks riwayat battle per pemain
#define BASE_DMG        10    // damage dasar tanpa senjata/XP
#define BASE_HP         100   // HP dasar tanpa XP
#define MATCHMAKE_TIMEOUT 35  // detik tunggu (tidak dipakai lagi, diganti 20)
```

### Struct Weapon
```c
typedef struct {
    char name[24];    // nama senjata ("Wood Sword", dll)
    int  bonus_dmg;   // bonus damage yang ditambahkan ke serangan
    int  price;       // harga dalam gold
} Weapon;
```
Tabel senjata adalah `static const` — artinya tersimpan di binary,
tidak di shared memory, dan tidak bisa diubah saat runtime.

| Senjata     | Bonus DMG | Harga |
|-------------|-----------|-------|
| Wood Sword  | +5        | 100   |
| Iron Sword  | +15       | 250   |
| Steel Sword | +30       | 500   |
| Dragon Blade| +60       | 1200  |

### Struct HistoryEntry
```c
typedef struct {
    char opponent[MAX_NAME];  // nama lawan
    char result[8];           // "WIN" atau "LOSE"
    int  xp_gained;           // XP yang didapat dari battle ini
    char timestamp[32];       // waktu battle "YYYY-MM-DD HH:MM"
} HistoryEntry;
```
Dipakai sebagai array di dalam struct Player. Maksimal 20 entry,
berputar (circular) menggunakan modulo.

### Struct Player
```c
typedef struct {
    char  username[MAX_NAME];
    char  password[MAX_PASS];
    int   gold;               // mata uang untuk beli senjata
    int   level;              // dihitung dari XP / 100 + 1
    int   xp;                 // total XP, tidak pernah reset
    int   weapon_idx;         // indeks ke WEAPONS[], atau -1 jika tidak punya
    int   online;             // 0=offline, 1=di lobby, 2=in-battle
    int   in_battle;          // 1 jika sedang di battle slot
    int   battle_slot;        // nomor slot battle yang sedang diikuti
    HistoryEntry history[MAX_HISTORY];
    int   history_count;      // total battle yang pernah dilakukan
} Player;
```
Struct ini disimpan di **shared memory** sehingga bisa dibaca oleh
semua client secara langsung (misalnya untuk cek HP lawan).

### Struct Battle
```c
typedef struct {
    int   active;        // 1 = battle sedang berjalan
    int   p1_idx;        // indeks Player di array players[]
    int   p2_idx;        // indeks Player, -1=BOT, -2=menunggu lawan
    int   p1_hp;         // HP saat ini p1
    int   p2_hp;         // HP saat ini p2
    int   p1_max_hp;     // HP maksimal p1 (tidak berubah selama battle)
    int   p2_max_hp;     // HP maksimal p2
    int   p1_dmg;        // damage per serangan p1
    int   p2_dmg;        // damage per serangan p2
    int   winner;        // 0=belum ada, 1=p1 menang, 2=p2 menang
    long  waiting_pid;   // PID client p1 saat status -2 (tidak aktif dipakai)
    int   react_target;  // 0=tidak ada challenge, 1=p1 yang harus menjawab
    time_t react_time;   // waktu challenge dimulai (untuk hitung sisa waktu)
    int   react_answered;// 1 = player sudah tekan huruf (benar atau salah)
    char  react_char;    // huruf yang harus ditekan player
    int   react_round;   // counter ronde, naik tiap challenge selesai
} Battle;
```
Nilai khusus `p2_idx`:
- `>= 0` → indeks pemain nyata
- `-1`   → lawan adalah BOT
- `-2`   → slot sedang MENUNGGU pemain kedua (fase matchmaking)

### Struct SharedArena
```c
typedef struct {
    Player  players[MAX_PLAYERS];   // semua pemain terdaftar
    int     player_count;           // jumlah pemain aktif
    Battle  battles[MAX_BATTLES];   // maks 4 battle bersamaan
} SharedArena;
```
Ini adalah layout lengkap shared memory. Ukurannya dihitung dengan
`sizeof(SharedArena)` dan digunakan saat `shmget()`.

### Konstanta Message Queue (Command Types)
```c
#define MSG_REGISTER   1   // client minta register akun baru
#define MSG_LOGIN      2   // client minta login
#define MSG_LOGOUT     3   // client minta logout
#define MSG_BATTLE_REQ 4   // client minta masuk antrian matchmaking
#define MSG_ATTACK     5   // client menyerang lawan
#define MSG_REACT_OK   6   // client menekan huruf reaction challenge
#define MSG_BUY        7   // client membeli senjata di armory
#define MSG_BOT_REQ    8   // client timeout matchmaking, minta bot
#define MSG_RESPONSE   100 // offset untuk response dari server ke client
```
Encoding mtype: `mtype = pid_client * 10 + cmd`
Contoh: client PID 1234 kirim LOGIN → mtype = 12342
Response ke client: mtype = 1234 + 100 = 1334

### Struct ArenaMsg
```c
typedef struct {
    long mtype;      // tipe pesan (encoding di atas)
    char data[256];  // isi pesan (string teks)
} ArenaMsg;
```
Format `data` berbeda tiap command, misalnya:
- LOGIN: `"username password"`
- ATTACK: `"player_idx slot"`
- REACT_OK: `"player_idx slot huruf"`

### Helper Semaphore (inline)
```c
static inline void sem_lock(int semid) {
    struct sembuf sb = {0, -1, 0};   // kurangi nilai semaphore 1
    semop(semid, &sb, 1);            // operasi blocking jika nilai = 0
}
static inline void sem_unlock(int semid) {
    struct sembuf sb = {0, 1, 0};    // tambah nilai semaphore 1
    semop(semid, &sb, 1);
}
```
`semop` dengan nilai `-1` = LOCK (tunggu sampai semaphore > 0).
`semop` dengan nilai `+1` = UNLOCK (bebaskan semaphore).
Ini memastikan hanya satu proses yang bisa mengubah shared memory
pada satu waktu, mencegah **race condition**.

### Helper Terminal Raw Mode
```c
static inline void set_raw_mode(void) {
    // simpan setting lama, lalu set ICANON=off, ECHO=off
    // VMIN=0 VTIME=0 = non-blocking read karakter
}
static inline void restore_terminal(void) {
    // kembalikan setting terminal ke semula
}
```
Dipakai selama battle agar input tidak perlu tekan ENTER,
dan karakter langsung terbaca satu per satu.

---

## orion.c — Server

### Variabel Global
```c
static int     shmid, msgid, semid;  // ID resource IPC
static SharedArena *arena;           // pointer ke shared memory
```

### save_data() dan load_data()
```c
void save_data(void)
```
Menulis `player_count` dan seluruh array `players[]` ke file biner
`eterion_save.dat`. Dipanggil setiap kali ada perubahan penting
(register, beli senjata, akhir battle).

```c
void load_data(void)
```
Membaca file save saat server pertama kali start. Setelah baca,
semua flag `online` dan `in_battle` di-reset ke 0 karena semua
koneksi sebelumnya sudah tidak valid.

### find_player()
```c
int find_player(const char *name)
```
Linear search di `arena->players[]` berdasarkan username.
Mengembalikan indeks atau -1 jika tidak ditemukan.

### send_resp()
```c
void send_resp(long pid, const char *msg)
```
Mengirim response ke client dengan PID tertentu.
`mtype = pid + MSG_RESPONSE` (= pid + 100) agar client bisa
filter response miliknya sendiri dari message queue.

### calc_dmg() dan calc_hp()
```c
int calc_dmg(Player *p)   // BASE_DMG + (xp/50) + bonus_weapon
int calc_hp(Player *p)    // BASE_HP  + (xp/10)
```
Formula damage dan HP berdasarkan total XP dan senjata yang dimiliki.

### add_history()
```c
void add_history(Player *p, const char *opp, const char *res, int xp)
```
Menambah entry ke riwayat pemain. Menggunakan circular buffer dengan
`history_count % MAX_HISTORY` sebagai indeks, sehingga entry lama
tertimpa jika sudah lebih dari 20 battle.

### end_battle()
```c
void end_battle(int slot, int winner_side)
```
Dipanggil saat HP salah satu pihak <= 0. Langkah-langkah:
1. Hitung siapa winner (w) dan loser (l)
2. Winner: +50 XP, +120 Gold, hitung ulang level, catat history "WIN"
3. Loser:  +15 XP, +30 Gold, hitung ulang level, catat history "LOSE"
4. Set `b->winner`, `b->active = 0`
5. Reset flag `in_battle` kedua pemain ke 0, online kembali ke 1
6. Panggil `save_data()`

### bot_thread()
```c
void *bot_thread(void *arg)
```
Thread terpisah yang berjalan selama battle vs BOT berlangsung.
Loop utama:
1. Hitung `jeda` = 3 - (react_round / 5), minimum 1 detik.
   Makin banyak ronde, makin sering BOT menyerang.
2. `sleep(jeda)` — tunggu sebelum challenge berikutnya
3. Hitung `window` = 2.5 - (react_round / 3) * 0.3, minimum 1.0 detik.
   Makin banyak ronde, makin sempit waktu reaksi pemain.
4. Set `react_char` = huruf random dari a-z
5. Set `react_target = 1`, `react_time = sekarang`, `react_answered = 0`
   → ini tertulis di shared memory, client baca langsung
6. `usleep(window * 1000000)` — tunggu selama window
7. Cek `react_answered`:
   - Jika 0 (player gagal/salah/tidak tekan): `p1_hp -= p2_dmg`
   - Jika hp <= 0: panggil `end_battle(slot, 2)` lalu break
8. Reset `react_target = 0`, naikkan `react_round++`

### spawn_bot()
```c
void spawn_bot(int slot)
```
Wrapper yang malloc `BotArg`, isi `slot`, lalu `pthread_create`
dengan `bot_thread`. Thread di-detach agar tidak perlu di-join.

### handle_register()
```c
void handle_register(ArenaMsg *m, long pid)
```
1. Parse `data` → username, password
2. `sem_lock` — kunci shared memory
3. Cek duplikat username dengan `find_player()`
4. Cek `player_count < MAX_PLAYERS`
5. Isi struct Player baru dengan nilai default:
   gold=150, level=1, xp=0, weapon_idx=-1
6. `player_count++`
7. `save_data()` — tulis ke disk
8. `sem_unlock`, kirim "OK" atau "ERR"

### handle_login()
```c
void handle_login(ArenaMsg *m, long pid)
```
1. Parse username, password
2. `sem_lock`
3. `find_player()` — cari indeks
4. Validasi password dengan `strcmp()`
5. Cek `online == 0` — pastikan tidak double login
6. Set `online = 1`
7. `sem_unlock`
8. Kirim response: `"OK idx username gold level xp weapon_idx"`

### handle_logout()
```c
void handle_logout(ArenaMsg *m, long pid)
```
1. Parse `player_idx`
2. `sem_lock`, set `online = 0`, `sem_unlock`
3. Kirim "OK Logout"

### handle_queue() — MATCHMAKING
```c
void handle_queue(ArenaMsg *m, long pid)
```
Ini adalah inti dari sistem matchmaking non-blocking:

**Fase A — Cek apakah ada slot yang menunggu (p2_idx == -2):**
1. Loop semua `arena->battles[i]`
2. Jika `active == 1` AND `p2_idx == -2` AND `p1_idx != idx`:
   - Set `p2_idx = idx` (kita jadi p2)
   - Hitung HP dan DMG p2
   - Set `players[idx].in_battle = 1`
   - `sem_unlock`
   - Kirim response ke p2: `"MATCH slot p1_idx p2hp p2max p2dmg p1hp p1max"`
   - RETURN — selesai, tidak perlu buat slot baru

**Fase B — Tidak ada yang menunggu, buat slot baru:**
1. Cari slot kosong (`active == 0`)
2. Inisialisasi Battle: `p1_idx = idx`, `p2_idx = -2` (WAITING)
3. Hitung HP dan DMG p1
4. Set `players[idx].in_battle = 1`
5. `sem_unlock`
6. Kirim ke client: `"WAIT slot"` — server langsung selesai, tidak blocking
7. Client yang akan polling shared memory sendiri

### handle_bot()
```c
void handle_bot(ArenaMsg *m, long pid)
```
Dipanggil oleh client setelah timeout 20 detik tidak dapat lawan:
1. Parse `player_idx`, `slot`
2. `sem_lock`
3. Validasi: `active == 1` AND `p2_idx == -2` (masih waiting)
4. Set `p2_idx = -1`, isi HP/DMG untuk BOT (BASE_HP+20, BASE_DMG+3)
5. `sem_unlock`
6. `spawn_bot(slot)` — mulai thread bot
7. Kirim response: `"OK slot -1 p1hp p1max p1dmg p2hp p2max"`

### handle_attack()
```c
void handle_attack(ArenaMsg *m, long pid)
```
1. Parse `player_idx`, `slot`
2. Cek `b->active`
3. `sem_lock`
4. Jika player adalah p1: kurangi `p2_hp -= p1_dmg`
   Jika player adalah p2: kurangi `p1_hp -= p2_dmg`
5. Jika HP <= 0: `sem_unlock`, panggil `end_battle()`
6. `sem_unlock`

### handle_react()
```c
void handle_react(ArenaMsg *m, long pid)
```
1. Parse `player_idx`, `slot`, `pressed` (huruf yang ditekan)
2. Cek `b->active`
3. `sem_lock`
4. Validasi tiga kondisi sekaligus:
   - `react_target != 0` (ada challenge aktif)
   - `react_answered == 0` (belum dijawab)
   - `pressed == react_char` (huruf TEPAT)
5. Jika semua valid: set `react_answered = 1`, kurangi HP lawan
6. Cek HP <= 0 → `end_battle()`
7. `sem_unlock`

Catatan: Jika huruf salah, `react_answered` TIDAK diset ke 1.
Bot thread akan tetap menunggu sampai window habis, lalu
menghukum player (kurangi HP player).

### handle_buy()
```c
void handle_buy(ArenaMsg *m, long pid)
```
1. Parse `player_idx`, `weapon_idx`
2. Validasi range weapon_idx (0–3)
3. `sem_lock`
4. Cek gold cukup
5. Cek senjata baru lebih kuat dari yang dimiliki
6. Kurangi gold, set `weapon_idx`
7. `save_data()`, `sem_unlock`
8. Kirim "OK" dengan nama senjata dan sisa gold

### cleanup() dan main()
```c
void cleanup(int sig)
```
Signal handler untuk SIGINT (Ctrl+C) dan SIGTERM:
1. `shmdt(arena)` — lepas mapping shared memory
2. `shmctl(IPC_RMID)` — hapus shared memory dari kernel
3. `msgctl(IPC_RMID)` — hapus message queue
4. `semctl(IPC_RMID)` — hapus semaphore
5. `exit(0)`

```c
int main(void)
```
1. Pasang signal handler
2. `shmget` + `shmat` → buat dan attach shared memory
3. `memset(arena, 0, ...)` → bersihkan shared memory
4. `msgget` → buat message queue
5. `semget` + `semctl(SETVAL, 1)` → buat semaphore, nilai awal 1
6. `load_data()` → muat data pemain dari disk
7. Tampilkan banner ASCII
8. **Loop utama**: `msgrcv(type=0)` — terima pesan APAPUN
9. Decode `cmd = mtype % 10`, `pid = mtype / 10`
10. Dispatch ke handler yang sesuai

---

## eternal.c — Client

### Variabel Global
```c
static int         msgid, shmid, semid;  // ID IPC (join, bukan create)
static SharedArena *arena;               // pointer ke shared memory server
static pid_t       mypid;               // PID proses ini, untuk routing pesan
```

### send_cmd()
```c
void send_cmd(int cmd, const char *data)
```
Mengirim perintah ke server:
- `mtype = mypid * 10 + cmd`
- Ini memungkinkan server decode siapa pengirim (mypid) dan
  apa perintahnya (cmd)

### wait_response()
```c
char *wait_response(void)
```
Blocking — menunggu response dari server dengan filter
`mtype = mypid + MSG_RESPONSE`. Mengembalikan pointer ke
buffer static berisi teks response.

### Konstanta Warna Terminal (ANSI Escape Codes)
```c
#define CLR  "\033[2J\033[H"   // clear screen + pindah cursor ke atas
#define BOLD "\033[1m"          // teks tebal
#define YEL  "\033[33m"         // kuning
#define CYN  "\033[36m"         // cyan
#define RED  "\033[31m"         // merah
#define GRN  "\033[32m"         // hijau
#define MAG  "\033[35m"         // magenta
#define RST  "\033[0m"          // reset ke default
```

### draw_hp_bar()
```c
void draw_hp_bar(int hp, int maxhp, int width)
```
Menggambar bar HP dengan karakter `█` dan `░`:
- Hitung `filled = hp * width / maxhp`
- Warna berubah: hijau (>50%), kuning (>25%), merah (<=25%)
- Format output: `[████████░░░░] 80/100`

### print_banner()
```c
void print_banner(void)
```
Clear screen lalu cetak logo ASCII "ETERION" dalam warna cyan,
diikuti garis pemisah. Dipanggil di awal setiap menu.

### kbhit()
```c
int kbhit(void)
```
Cek apakah ada input keyboard yang tersedia TANPA blocking,
menggunakan `select()` dengan timeout 0. Mengembalikan 1 jika
ada karakter di buffer stdin, 0 jika tidak ada.
Dipakai di battle loop agar program tidak berhenti menunggu input.

### Struct BattleCtx
```c
typedef struct {
    int    slot;       // nomor battle slot di shared memory
    int    pidx;       // indeks player kita di arena->players[]
    int    is_bot;     // 1 jika lawan adalah BOT
    int    running;    // flag loop battle, 0 = hentikan
    char   log1[128];  // baris combat log terbaru
    char   log2[128];  // baris combat log sebelumnya
    time_t last_atk;   // waktu terakhir tekan 'a' (untuk cooldown 1 detik)
} BattleCtx;
```
Variabel global `static BattleCtx bctx` dipakai bersama oleh
main battle loop dan render thread.

### draw_battle()
```c
void draw_battle(void)
```
Fungsi render utama, dipanggil 5x/detik oleh render thread:
1. Baca data langsung dari shared memory (HP, react_target, dll)
2. Tentukan `my_hp`, `opp_hp`, `opp_name` berdasarkan apakah
   kita p1 atau p2 di slot ini
3. Clear screen, cetak header "ARENA BATTLE"
4. Cetak card lawan (merah) dengan HP bar
5. Cetak card kita (hijau) dengan HP bar
6. Cek `b->react_target != 0 && !b->react_answered`:
   - **Ya** → hitung sisa waktu, tampilkan box challenge
     dengan warna sesuai urgensi:
     - Hijau  : sisa > 60% window ("⚡ Tekan [ X ] sekarang!")
     - Kuning : sisa > 30% window ("⚠  Tekan [ X ] cepat!")
     - Merah  : sisa <= 30% window ("🔥 SEKARANG!! [ X ] !!")
   - **Tidak** → tampilkan "[a] Attack   [q] Quit"
7. Cetak 2 baris combat log
8. Tampilkan cooldown serangan: READY (hijau) atau 1.0s (kuning)

### push_log()
```c
void push_log(const char *msg)
```
Geser log: log1 → log2, lalu isi log1 dengan pesan baru.
Efeknya adalah tampilan dua baris log yang bergulir ke bawah.

### battle_render_thread()
```c
void *battle_render_thread(void *arg)
```
Thread yang hanya bertugas render. Loop:
- Panggil `draw_battle()`
- `usleep(200000)` — tunggu 0.2 detik (refresh rate 5fps)
- Ulangi selama `bctx.running == 1`

### do_battle()
```c
void do_battle(int slot, int pidx, int opp_idx)
```
Fungsi utama battle. Langkah:
1. Inisialisasi `bctx`
2. `set_raw_mode()` — aktifkan mode terminal tanpa ENTER
3. `pthread_create(&render_tid, battle_render_thread)` — mulai render
4. **Loop utama** (berjalan di main thread):
   - Cek `b->active` — jika 0 (battle selesai dari server/bot thread),
     set `running = 0` dan break
   - `kbhit()` — cek input non-blocking:
     - `'q'` → set `running = 0`
     - Huruf apapun SAAT challenge aktif (`react_target != 0`) →
       kirim `MSG_REACT_OK` ke server dengan huruf yang ditekan.
       Tampilkan log "✅ COUNTER!" jika benar atau "❌ Salah!" jika keliru.
     - `'a'` SAAT tidak ada challenge →
       Cek cooldown (>= 1 detik sejak last_atk).
       Jika ready: kirim `MSG_ATTACK`, update `last_atk`, log serangan.
       Jika cooldown: log "Cooldown! Tunggu sebentar..."
   - `usleep(50000)` — polling 20x/detik
5. `pthread_join(render_tid)` — tunggu render thread selesai
6. `restore_terminal()` — kembalikan mode terminal normal
7. Cetak hasil battle (VICTORY atau DEFEATED) berdasarkan `b->winner`

### menu_armory()
```c
void menu_armory(int pidx)
```
1. Tampilkan list senjata dengan tanda `*` pada senjata aktif
2. Tampilkan gold saat ini
3. Input pilihan (1–4 atau 0 untuk kembali)
4. Kirim `MSG_BUY` ke server
5. Tampilkan response (berhasil atau error)

### menu_history()
```c
void menu_history(int pidx)
```
1. Baca `p->history[]` dari shared memory
2. Tampilkan tabel: Lawan, Hasil (WIN=hijau/LOSE=merah), XP, Waktu
3. Jika belum ada riwayat, tampilkan pesan kosong

### menu_lobby()
```c
void menu_lobby(int pidx)
```
Loop menu utama setelah login:
1. Tampilkan profile card (nama, level, gold, XP, senjata)
2. Menu: Battle, Armory, Riwayat, Logout

**Jika pilih Battle:**
1. Kirim `MSG_BATTLE_REQ` ke server
2. `wait_response()` — tunggu jawaban server
3. Jika response `"ERR"` → tampilkan error, kembali ke lobby
4. Jika response `"MATCH slot ..."` → langsung masuk `do_battle()`
   (ada pemain lain yang sudah menunggu)
5. Jika response `"WAIT slot"` → masuk loop polling SHM:
   - Loop 20 kali (20 detik), tiap iterasi:
     - Cetak `.` animasi
     - `sleep(1)`
     - Cek `arena->battles[slot].p2_idx >= 0`:
       jika ada pemain join → masuk `do_battle()`
   - Jika loop habis tanpa lawan → kirim `MSG_BOT_REQ` ke server,
     tunggu response "OK", lalu `do_battle()` dengan BOT

### do_register()
```c
void do_register(void)
```
1. Input username dan password via `scanf`
2. Kirim `MSG_REGISTER`
3. Tampilkan hasil: "✔ Berhasil!" atau "✘ [error]"

### do_login()
```c
int do_login(void)
```
1. Input username dan password
2. Kirim `MSG_LOGIN`
3. Jika berhasil: parse response, kembalikan `player_idx`
4. Jika gagal: kembalikan -1

### main() — Client
```c
int main(void)
```
1. `mypid = getpid()` — simpan PID untuk routing pesan
2. **Cek server aktif**: panggil `msgget`, `shmget`, `semget` TANPA
   `IPC_CREAT`. Jika gagal (server belum jalan), tampilkan error
   `"❌ Orion are you there?"` dan keluar.
3. `shmat` — attach ke shared memory server
4. Loop menu utama: Register / Login / Exit
5. `shmdt(arena)` sebelum exit

---

## Alur Lengkap Program

### 1. Server Start
```
./orion
  ↓
shmget(IPC_CREAT) → alokasi shared memory baru di kernel
shmat()           → map ke address space server
memset(arena, 0)  → bersihkan seluruh shared memory
msgget(IPC_CREAT) → buat message queue
semget(IPC_CREAT) → buat semaphore, nilai awal = 1
load_data()       → baca eterion_save.dat jika ada
while(1) msgrcv() → tunggu pesan dari client manapun
```

### 2. Client Start
```
./eternal
  ↓
getpid()          → simpan mypid (mis. 5678)
msgget(0666)      → JOIN message queue yang sudah ada (tanpa IPC_CREAT)
shmget(0666)      → JOIN shared memory yang sudah ada
semget(0666)      → JOIN semaphore yang sudah ada
shmat()           → map shared memory ke address space client
menu utama        → tampilkan Register/Login/Exit
```

### 3. Register
```
Client input username + password
  ↓
send_cmd(MSG_REGISTER, "rafi secret123")
  mtype = 5678 * 10 + 1 = 56781
  ↓ (masuk message queue)
Server msgrcv() → cmd=1(REGISTER), pid=5678
  sem_lock()
  find_player("rafi") → -1 (belum ada)
  buat Player baru, player_count++
  save_data() → tulis ke disk
  sem_unlock()
  send_resp(5678, "OK Register berhasil")
  mtype = 5678 + 100 = 5778
  ↓ (masuk message queue)
Client wait_response() → filter mtype=5778
  tampilkan "✔ Berhasil!"
```

### 4. Login
```
Client send_cmd(MSG_LOGIN, "rafi secret123")
  ↓
Server: find_player → cek password → set online=1
  send_resp: "OK 0 rafi 150 1 0 -1"
  (idx=0, gold=150, level=1, xp=0, weapon=-1)
  ↓
Client: parse idx → masuk menu_lobby(0)
```

### 5. Matchmaking PvP (2 Client)
```
Client A (PID 5678, idx=0) pilih Battle:
  send_cmd(MSG_BATTLE_REQ, "0")
  ↓
Server handle_queue():
  Tidak ada slot waiting
  Buat battles[0]: p1_idx=0, p2_idx=-2 (WAITING)
  players[0].in_battle=1
  send_resp(5678, "WAIT 0")
  ↓
Client A terima "WAIT 0":
  Loop polling: cek arena->battles[0].p2_idx tiap 1 detik
  Cetak "........."

Client B (PID 9999, idx=1) pilih Battle:
  send_cmd(MSG_BATTLE_REQ, "1")
  ↓
Server handle_queue():
  Temukan battles[0] dengan p2_idx==-2 dan p1_idx!=1
  Set battles[0].p2_idx=1
  Set battles[0].p2_hp, p2_dmg
  players[1].in_battle=1
  send_resp(9999, "MATCH 0 0 110 110 12 105 105")
  ↓
Client B terima "MATCH":
  Langsung do_battle(slot=0, pidx=1, opp_idx=0) ✅

Client A (masih polling SHM):
  Deteksi battles[0].p2_idx == 1 (>= 0)
  do_battle(slot=0, pidx=0, opp_idx=1) ✅
```

### 6. Matchmaking vs BOT (Timeout)
```
Client A polling selama 20 detik, tidak ada yang join:
  send_cmd(MSG_BOT_REQ, "0 0")
  ↓
Server handle_bot():
  battles[0].p2_idx = -1 (BOT)
  p2_hp = 120, p2_dmg = 13
  spawn_bot(0) → pthread_create(bot_thread)
  send_resp(pid, "OK 0 -1 105 105 12 120 120")
  ↓
Client A:
  do_battle(slot=0, pidx=0, opp_idx=-1) ✅
```

### 7. Battle Berlangsung
```
do_battle() start:
  set_raw_mode()     → terminal tanpa ENTER
  pthread_create()   → render thread mulai draw_battle() 5fps

Main loop (20x/detik via usleep 50ms):
  ┌─ cek b->active == 0? → battle sudah selesai, break
  └─ kbhit()?
      ├─ 'q' → running=0
      ├─ huruf saat challenge aktif → send MSG_REACT_OK
      └─ 'a' saat tidak ada challenge → send MSG_ATTACK (cooldown 1 detik)

Bot thread (berjalan paralel di server):
  sleep(jeda) → set react_char, react_target=1 di SHM
  usleep(window) → cek react_answered:
    - 0 (gagal): p1_hp -= p2_dmg
    - Jika p1_hp <= 0: end_battle(slot, 2)
  react_target=0, react_round++

Render thread (berjalan paralel di client):
  Tiap 200ms: baca SHM → draw_battle() → refresh layar
  Deteksi react_target → tampilkan box challenge dengan countdown

End battle (ketika HP <= 0):
  Server set b->active=0, b->winner=1 atau 2
  Client main loop: deteksi b->active==0 → running=0
  pthread_join(render_tid)
  restore_terminal()
  Tampilkan VICTORY atau DEFEATED
```

---

## Sistem IPC

### Shared Memory
**Tujuan**: Berbagi state game secara real-time antar proses tanpa
overhead message passing.

Client membaca SHM **langsung** untuk:
- HP pemain dan lawan (untuk HP bar)
- `react_target`, `react_char`, `react_time` (untuk challenge display)
- `b->active` (untuk deteksi akhir battle)
- `p2_idx` (untuk polling matchmaking)

Server menulis SHM untuk:
- Data pemain (login, register, buy, end_battle)
- State battle (HP, challenge fields)

### Message Queue
**Tujuan**: Pengiriman perintah dan response dengan routing per-PID.

Encoding mtype:
```
Client → Server:  mtype = pid * 10 + command_code
Server → Client:  mtype = pid + 100
```

Contoh routing dengan 2 client (PID 5678 dan 9999):
```
Queue berisi pesan dengan mtype:
  56781  (dari 5678, command LOGIN)
  99992  (dari 9999, command REGISTER)
  5778   (response untuk 5678)
  10099  (response untuk 9999)

Client 5678 panggil msgrcv(mtype=5778) → hanya terima miliknya
Client 9999 panggil msgrcv(mtype=10099) → hanya terima miliknya
Server panggil msgrcv(mtype=0) → terima semua pesan
```

### Semaphore
**Tujuan**: Mutual exclusion (mutex) untuk mencegah race condition.

Nilai semaphore: 1 = bebas, 0 = sedang dipakai.

Pattern penggunaan:
```c
sem_lock(semid);     // jika nilai=0, tunggu. jika =1, kurangi jadi 0
// ... ubah shared memory ...
sem_unlock(semid);   // tambah nilai ke 1, bebaskan
```

Tanpa semaphore, dua proses bisa membaca-menulis SHM bersamaan
dan data bisa korup (misalnya HP terbaca setengah-update).

---

## Sistem Matchmaking

### State Diagram p2_idx
```
-2 = WAITING (menunggu lawan)
-1 = BOT
≥0 = indeks pemain nyata

[WAITING -2]
    │
    ├── Ada pemain lain join dalam 20 detik
    │       └─→ [PvP: p2_idx = opp_idx]
    │
    └── Timeout 20 detik, tidak ada lawan
            └─→ [BOT: p2_idx = -1]
```

### Kenapa Non-Blocking?
Server adalah **single-threaded** di message loop. Jika server
melakukan `sleep()` sambil menunggu lawan untuk p1, server akan
**berhenti memproses** semua pesan dari client lain, termasuk
pesan dari p2 yang ingin bergabung — deadlock.

Solusi: Server hanya set state di SHM (`p2_idx = -2`) lalu
**langsung return**. Client p1 yang bertanggung jawab polling
SHM setiap 1 detik. Ketika p2 bergabung dan server mengubah
`p2_idx`, client p1 akan mendeteksinya sendiri.

---

## Sistem Battle

### Thread Model
```
[Process orion (server)]
  Main thread:  message loop (terima serangan, react, dll)
  Bot thread:   bot_thread() per battle vs BOT (pthread_detach)

[Process eternal (client)]
  Main thread:  input loop (kbhit, send attack/react)
  Render thread: draw_battle() tiap 200ms
```

### Challenge Lifecycle
```
1. bot_thread set di SHM:
   react_char='k', react_target=1, react_time=now, react_answered=0

2. Render thread (client) baca SHM:
   react_target==1 → tampilkan box "⚡ Tekan [K]"
   Hitung sisa = window - difftime(now, react_time)

3a. Jika player tekan 'k' (benar):
    send MSG_REACT_OK "pidx slot k"
    Server: pressed==react_char → react_answered=1, opp_hp -= dmg

3b. Jika player tekan huruf salah:
    send MSG_REACT_OK "pidx slot x"
    Server: pressed!=react_char → tidak ada efek
    Bot thread: tunggu sampai window habis → p1_hp -= p2_dmg

3c. Jika player tidak tekan apapun:
    Bot thread: react_answered==0 → p1_hp -= p2_dmg

4. Bot thread: react_target=0, react_round++
   Render thread: react_target==0 → tampilkan "[a] Attack"
```

### Difficulty Scaling
| react_round | Jeda (detik) | Window (detik) |
|-------------|--------------|----------------|
| 0–4         | 3            | 2.5            |
| 5–9         | 2            | 2.2            |
| 10–14       | 1            | 1.9            |
| 15–17       | 1            | 1.6            |
| 18–20       | 1            | 1.3            |
| 21+         | 1            | 1.0 (minimum)  |

---

## Formula Progressi

| Atribut | Formula |
|---------|---------|
| Damage  | `BASE_DMG(10) + (total_xp / 50) + bonus_weapon` |
| HP      | `BASE_HP(100) + (total_xp / 10)` |
| Level   | `(total_xp / 100) + 1` |
| XP Menang | +50 |
| XP Kalah  | +15 |
| Gold Menang | +120 |
| Gold Kalah  | +30 |

Contoh pemain dengan 500 XP dan Iron Sword (+15):
- Damage = 10 + (500/50) + 15 = **35**
- HP     = 100 + (500/10)     = **150**
- Level  = (500/100) + 1      = **6**
