# Benchmark VRB pada dataset 69

Menang, seri, dan kalah dihitung dari sudut pandang VRB pada pasangan hasil yang layak. Tanda hubung pada VRB menunjukkan baris acuan.

## Kualitas solusi

Gap rata-rata hanya memakai kasus layak dengan optimum positif.

| Metode | Layak | Gap rata-rata (%) | Optimal | Menang/seri/kalah VRB | Pasangan |
|---|---:|---:|---:|---:|---:|
| VAM | 69/69 | 3,565986 | 36 | 26/38/5 | 69 |
| SAM_original | 67/69 | 1,792396 | 48 | 14/51/2 | 67 |
| SAM_repaired | 69/69 | 2,115392 | 48 | 16/51/2 | 69 |
| RBSM | 69/69 | 2,473487 | 45 | 24/41/4 | 69 |
| Fractional | 69/69 | 3,983091 | 28 | 36/30/3 | 69 |
| VRB_lama | 69/69 | 0,789262 | 55 | 0/69/0 | 69 |
| VRB | 69/69 | 0,789262 | 55 | - | - |
| CSM | 69/69 | 3,783236 | 42 | 26/39/4 | 69 |
| NWCM_FIX | 69/69 | 44,041184 | 3 | 66/3/0 | 69 |
| LCM2 | 69/69 | 6,795846 | 14 | 51/16/2 | 69 |
| LCM2_sentinel_fixed | 69/69 | 6,795846 | 14 | 51/16/2 | 69 |
| CSM2_int64 | 69/69 | 3,789324 | 43 | 26/39/4 | 69 |
| CSM2_original | 69/69 | 3,789324 | 43 | 26/39/4 | 69 |
| RCWMCAM | 69/69 | 7,039858 | 21 | 46/23/0 | 69 |
| IHW | 69/69 | 4,441102 | 40 | 26/34/9 | 69 |
| EXP-ICHW | 69/69 | 4,398083 | 34 | 31/37/1 | 69 |
| TOCM-TDM1 | 69/69 | 2,112915 | 42 | 24/38/7 | 69 |
| BCE | 62/69 | 2,705478 | 41 | 19/39/4 | 62 |
| PowerRank | 69/69 | 3,780388 | 30 | 33/33/3 | 69 |
| JHM_langsung | 67/69 | 2,916807 | 40 | 23/40/4 | 67 |

## Waktu program lengkap

Ringkasan waktu memakai median pengulangan tiap kasus layak. Seri berarti median nanodetiknya identik.

| Metode | Median (ms) | Mean (ms) | Min (ms) | Max (ms) | Menang/seri/kalah VRB | Pasangan |
|---|---:|---:|---:|---:|---:|---:|
| VAM | 0,5899 | 0,6130 | 0,5572 | 0,9968 | 0/0/69 | 69 |
| SAM_original | 0,5866 | 0,5959 | 0,5451 | 0,8263 | 0/0/67 | 67 |
| SAM_repaired | 0,5883 | 0,6010 | 0,5523 | 0,7452 | 0/0/69 | 69 |
| RBSM | 0,6056 | 0,6183 | 0,5622 | 0,8477 | 0/0/69 | 69 |
| Fractional | 0,9252 | 0,9495 | 0,8364 | 1,2689 | 64/0/5 | 69 |
| VRB_lama | 0,6816 | 0,7156 | 0,6412 | 1,0973 | 2/0/67 | 69 |
| VRB | 0,8298 | 0,8438 | 0,7668 | 1,0787 | - | - |
| CSM | 0,5967 | 0,6147 | 0,5582 | 0,9012 | 0/0/69 | 69 |
| NWCM_FIX | 0,5932 | 0,6029 | 0,5500 | 0,8217 | 0/0/69 | 69 |
| LCM2 | 0,6390 | 0,6530 | 0,6020 | 0,9864 | 2/0/67 | 69 |
| LCM2_sentinel_fixed | 0,6419 | 0,6551 | 0,5947 | 0,8191 | 0/0/69 | 69 |
| CSM2_int64 | 0,5927 | 0,6083 | 0,5558 | 0,8422 | 0/0/69 | 69 |
| CSM2_original | 0,5927 | 0,6050 | 0,5571 | 0,7702 | 0/0/69 | 69 |
| RCWMCAM | 0,6743 | 0,7011 | 0,6291 | 1,0138 | 0/0/69 | 69 |
| IHW | 0,7822 | 0,8039 | 0,7243 | 1,1663 | 11/0/58 | 69 |
| EXP-ICHW | 0,6934 | 0,7153 | 0,6421 | 1,0131 | 1/0/68 | 69 |
| TOCM-TDM1 | 0,5970 | 0,6085 | 0,5571 | 0,7754 | 0/0/69 | 69 |
| BCE | 0,5910 | 0,5981 | 0,5385 | 0,7667 | 0/0/62 | 62 |
| PowerRank | 0,6064 | 0,6359 | 0,5619 | 0,9267 | 0/0/69 | 69 |
| JHM_langsung | 0,5950 | 0,6064 | 0,5577 | 0,7565 | 0/0/67 | 67 |
