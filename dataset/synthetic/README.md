# Dataset sintetis untuk evaluasi tie-breaker VRB

Sebanyak 3.000 instance independen, seed `2026110401`. Ada 10 keluarga biaya dan
10 kelompok ukuran, masing-masing 30 kasus per kombinasi. Setiap kombinasi
memiliki 15 kasus pengembangan dan 15 kasus holdout. Pembagian ditentukan
sebelum biaya dibuat. Jangan memilih aturan berdasarkan hasil holdout.

Seluruh ukuran baris dan kolom 3 sampai 100 muncul, termasuk 3 × 3 dan
100 × 100. Koleksi mencakup matriks persegi dan persegi panjang dengan
kedua orientasi. Ini tidak mencakup seluruh 9.604 pasangan dimensi.

Format TXT: `m n`, matriks biaya m × n, supply m angka, demand n angka.
Tidak ada nilai optimum yang ditempelkan. Biaya adalah bilangan bulat
nonnegatif. Sebanyak 450 kasus (15%) tidak seimbang, separuh kelebihan
supply dan separuh kelebihan demand. Solver harus menambahkan dummy
berbiaya nol sebelum reduksi. Sebanyak 150 kasus (5%) memiliki sedikitnya
satu margin nol; garis tersebut perlu dinonaktifkan.

Keluarga biaya: uniform_wide (1..1.000), uniform_narrow (10..30),
tied_discrete (0/1/2/5/10/20), near_equal (dasar + gangguan -2..2),
row_column_offsets (komponen baris/kolom besar + residual kecil),
block_cluster (lebih murah dalam kelompok), spatial_integer (jarak Manhattan),
correlated_low_rank (faktor laten integer), sparse_zeros (15..45% biaya nol),
dan bimodal (biaya rendah/tinggi). Keluarga yang sering seri sengaja disertakan.

Metadata dan SHA-256 setiap berkas tersedia di `manifest.json` dan `manifest.csv`.
Gunakan `local_path` untuk menemukan berkas dari direktori ini. Kolom `path`
menyimpan nama berkas pada eksperimen asal. Audit koleksi tersedia di `audit.json`.
Hasil eksperimen terdahulu ada di [arsip synthetic](../../results/synthetic).
