# DAYI_DELAY_SUMMARY

Bu dosya, gelecekteki pluginlerde PHASEUS Dayi Delay karakterini korumak icin kisa "design + feature DNA" hafizasidir.
Proje tanitimi degil; sadece uygulama disiplini ve imza kararlar.

## 1) Tasarim Dili (Non-Negotiable)

- Full dark, high contrast, okunakli tipografi.
- Minimal ama "steril" degil: analog hissi + modern duzen.
- Header sabit ve tekrar kullanilabilir:
  - Sol: plugin kimligi
  - Orta: preset sistemi
  - Sag: hizli global utility (gain, toggle vs.)
- Buton geometrisi: square (radius 0).
- Kontrol yogunlugunda "ilk kural":
  - once sade duzen,
  - sonra conditionally visible alanlar,
  - en son scrollbar fallback.

## 2) Etkilesim Standartlari

- Double-click => default value.
- Text entry acik olmali (numeric hizli giris).
- Sync acikken ana time kontrolu kaybolmaz; sadece sunum muziksel division olur.
- Linkli parametrelerde (L/R) bagli degilse zincir ikonu da gorunmez.
- Dropdown/popup listelerde:
  - uzun listede scroll kesin,
  - kritik pseudo itemler (open folder gibi) listede bulunur.

## 3) DSP Imzasi (Audio Karakteri)

- Wet path ile dry path mantiksal olarak ayrik tutulur.
- Filter/reverse/diffusion/lofi gibi "renklendirici" bloklar varsayilan olarak wet tarafi etkiler.
- Randomizasyon "continuous jitter" degil, musical event/periyot bazli hold mantiginda calisir.
- Stereo hareketlerde constant-power pan kullanilir (enerji korunumu).
- Pop/click riskli yerlerde:
  - smoothing,
  - envelope/windowing,
  - clamp/safety limit zorunlu.

## 4) Preset Sistemi Kurallari

- Preset save/load her zaman APVTS state uzerinden.
- Eski preset + yeni parametre uyumu bozulmaz:
  - presette olmayan parametre defaultta kalir.
- UI secili preset adini dogru yansitir:
  - degisiklik varsa `*` dirty indicator.
- Init bir "pseudo/default state" olarak her zaman mevcut kalir.

## 5) Gelecek Pluginlerde Kopyalanacak Ozellik Paketi

- Reusable preset bar component.
- Mode tabs (combobox yerine net mod secimi).
- L/R link + chain icon kurali.
- Musical sync divisions (straight/dotted/triplet).
- Compact knob+random ikilisi (layout tasirmadan).
- Saglam fallback:
  - overflow -> scroll,
  - eski preset -> default fallback,
  - param migration -> crashsiz.

## 6) Kaçinilacak Hatalar (Tekrar Etme)

- Audio thread icinde allocation/lock.
- Parametreyi atomic'ten `load()` etmeden kullanmak.
- Random/time parametrelerini her block/sample salinima sokmak.
- UI'da kontrolu tamamen gizleyip kullanicinin state baglamini kaybettirmek.
- Cross-platform dialog boyut/odak farklarini test etmeden birakmak.

## 7) "Done" Olmadan Release Etme

- DAW icinde preset save/load + reopen project testi.
- Sync on/off ve link on/off davranis testi.
- Grain/pingpong modlarinda pop/click/noise regression kontrolu.
- Windows + macOS UI basic smoke test (modal, popup, text entry).
- VST3 artifact packaging path CI'da dogrulama.
