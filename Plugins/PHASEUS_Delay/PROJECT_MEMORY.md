# PROJECT_MEMORY (Phaseus)

Bu dosya, bu repoda yapilan tum kritik degisikliklerin, hatalarin ve cozumlerin kalici notudur.

## 1) Kurulan Sistem

- Monorepo yapisi:
  - Tek ortak `JUCE/`
  - Pluginler `Plugins/<PluginName>/`
- Otomatik plugin kesfi:
  - `Plugins/CMakeLists.txt` alt klasorleri tarar, `CMakeLists.txt` olanlari ekler.
- Template + scaffold:
  - `Templates/DefaultPluginTemplate/`
  - `./createplugin <PluginName>`

## 2) Eklenen Araclar

- `createplugin`
  - Template kopyalar, placeholder doldurur.
- `build_and_copy`
  - Configure + build + VST3 copy target tek komut.
  - Kullanim: `./build_and_copy PHASEUS_Delay`

## 3) PHASEUS_Delay Ozellikleri

- Modlar:
  - `Simple`, `PingPong`, `Grain`
- Parametre yapisi:
  - APVTS tabanli, state save/load aktif.
- Sync:
  - DAW BPM sync (note division: `1/1, 1/2, 1/4, 1/8, 1/16, 1/32, dotted, triplet`)
  - Simple ve PingPong icin sync toggle + division.
- PingPong baglantilar:
  - `Sync L/R Time`
  - `Sync L/R Feedback`
  - Link acikken L/R beraber hareket.

## 4) UI Kimligi (Signature)

- Hedef dil:
  - Minimal, modern, future-garage, siyah-beyaz odak.
- Davranis:
  - Mod bazli dinamik kontrol gorunumu.
  - Wet/Dry global kontrol.
  - Full dark mode standardi (toggle yok).
  - Double-click default deger.
  - Text box ile klavyeden deger girisi.

## 5) Karsilasilan Hatalar ve Fixler

1. `JuceHeader.h file not found`
- Neden: `juce_generate_juce_header()` yoktu.
- Fix: Plugin ve template CMake dosyalarina eklendi.

2. `std::atomic<float>` copy / `jlimit` type conflict hatalari
- Neden: `getRawParameterValue()` sonucu dogrudan kopyalaniyordu.
- Fix: tum parametre okumalar `->load()` ile yapildi.

3. `processor shadows inherited member` warning
- Neden: Editor'de `processor` ismi JUCE base member ile cakisti.
- Fix: `audioProcessor` olarak degistirildi.

4. VST2/VST3 automation conflict compile error
- Neden: JUCE VST3 koruma check.
- Fix: `JUCE_VST3_CAN_REPLACE_VST2=0`.

5. Grain modda pop/click/pit pit
- Neden: anlik grain ac/kapa ve sert gecis.
- Fix:
  - Grain envelope (windowing)
  - Wet smoothing
  - Daha kontrollu retrigger ve feedback clamp

6. `getCurrentPosition` deprecated warning
- Neden: eski playhead API kullanimi.
- Fix: `getPosition()` + `getBpm()` API'sine gecildi.

7. Build sonrasi plugin kopyalanmiyor
- Neden: copy adimi bazen up-to-date akista tetiklenmiyor/karisiyor.
- Fix:
  - VST3 explicit copy dir
  - `CopyVST3` custom target (ALL)
  - `build_and_copy` scripti

8. DAW'da eski UI gorunmesi
- Neden: AU/VST3 karisimi veya cache.
- Fix:
  - PHASEUS_Delay `FORMATS VST3 Standalone` (AU kapatildi)
  - DAW restart + scan
  - Gerekirse eski AU bundle silinmeli

## 6) Format ve Dagitim Notlari

- PHASEUS_Delay su an VST3 odakli:
  - `FORMATS VST3 Standalone`
- Kopya hedefi:
  - `~/Library/Audio/Plug-Ins/VST3/PHASEUS_Delay.vst3`

## 7) Gelecekte Yeni Plugin Acarken Checklist

1. `./createplugin <Name>`
2. Mode + APVTS parametrelerini planla
3. Realtime kurallar:
  - no allocation/no lock in `processBlock`
4. Parametre okumada daima `->load()`
5. `juce_generate_juce_header()` oldugunu dogrula
6. VST3 replacement policyyi belirle (`JUCE_VST3_CAN_REPLACE_VST2`)
7. UI:
  - Header sabit
  - Mod bazli gorunum
  - Double-click default + editable textbox
8. Build + copy:
  - `./build_and_copy <Name>`
9. DAW smoke test:
  - load, automation, preset/state, sync, artifact kontrol

## 8) PHASEUS Icin Korunacak Imza Kararlar

- Mod odakli sade panel
- Muziksel sync division dili
- L/R link semantigi (time/feedback)
- Yuksek okunabilirlik + kontrast
- Realtime-safe DSP onceligi

## 9) Son Iterasyon Notlari (UI + DSP)

### UI tarafinda eklenenler

- Delay mode combobox -> tab sistemi (Simple/PingPong/Grain).
- Filter alani modernize edildi:
  - Type tabs (`Off/LP/HP/Notch/Comb`)
  - Interaktif filter response graph
  - Horizontal sliderlar
  - `Comb` secilince sadece comb parametreleri gorunur.
- Sag panel scroll altyapisi:
  - Dikey scrollbar
  - Wheel ile scroll
  - Overflow durumunda panel tasmadan gezilebilir.
- Header optimize:
  - Daha kisa yukseklik
  - `Dark` yanina `LoFi` toggle eklendi.

### DSP tarafinda eklenenler

- Grain ping-pong pan parametresi:
  - `grainPingPongPan`
  - Constant-power pan ile uygulama (enerji dengeli L/R).
- LoFi wet processing:
  - Sadece wet/delay output yoluna uygulanir.
  - Basit bit-depth quantize + sample hold/downsample karakteri.

### Kritik bug ve cozumleri

1. `Time Random` noise/click sorunu
- Semptom: Delay time random parametresi sesi "surekli kiriyor", noise/jitter gibi davranis.
- Koken: Time random her blockta yeniden hesaplandigi icin read pointer durmadan zipliyor.
- Cozum:
  - "Held random" modeli eklendi.
  - Random time bir echo periyodu boyunca sabit, periyot sonunda yenileniyor.
  - Simple/PingPong icin ayri counter + held value tutuluyor.

2. Grain ping-pong pan kontrolu gorunmeme sorunu
- Semptom: Pan kontrolu UI'da kayboluyor/bozuk gorunuyor.
- Koken: Rotary layout yukseklik/tasima ile cakisma.
- Cozum:
  - Pan kontrolu linear slider + sag panel satir layout.
  - Gorunurluk sadece Grain + PingPong output acikken.

3. Scroll "hissi" zayif sorunu
- Semptom: Sag panelde scroll var gibi ama belirgin degil.
- Cozum:
  - Scrollbar kalinlastirildi ve kontrast arttirildi.
  - Auto-hide kapatildi.
  - Wheel hit-area genisletildi.

### Korunacak implementasyon prensipleri (gelecek pluginler)

- DSP randomizasyonunda "musical rate" kullan:
  - Her sample degil, olay/periyot bazli random.
- UI'da mode switch oldugunda:
  - ilgili kontrolleri gostergede tut, tamamen kaybetme (ozellikle ana knoblar).
- Sag panelde yeni ozellik eklendikce:
  - once compact layout + conditional visibility,
  - sonra scrollbar fallback.

## 10) Son Eklenenler (Stabil Snapshot)

### DSP

- Reverse path fix:
  - Reverse okuma kaynagi `delayBuffer` yerine ayri `reverseBuffer` (wet history) oldu.
  - Sonuc: reverse artik inputu degil delay/grain wet karakterini ters ceviriyor.
- Reverse window controls:
  - `Reverse Start Offset` + `Reverse End Offset` aktif.
  - Reverse usable length: `target - start - end`, minimum 1 sample clamp.
- Diffusion:
  - Wet signal uzerinde 2-stage allpass diffusion eklendi.
  - Parametreler: `Diffusion Enable`, `Diffusion Amount`, `Diffusion Size`.
- Output stage:
  - Global `Output Gain (dB)` eklendi, final cikisa uygulanir.

### UI / UX

- Full dark standard:
  - Dark/light switch tamamen kaldirildi.
- Header sistemi:
  - Sol: plugin title
  - Orta: preset bar (shared)
  - Sag: output gain + hizli utility toggle(lar)
- Preset save modal:
  - Dark-themed
  - Her zaman merkeze spawn olur.
- Button standard:
  - Butun `TextButton` tabanli butonlar square style (radius 0).
- Background:
  - Binary asset'ten yuklenir, dark overlay ile okunabilirlik korunur.
- Link icon:
  - Ping/Pong link gorseli asset ile cizilir (unicode karakter yerine).

## 11) Kritik Hata/Fix Kaydi (Yeni)

1. Background gorunmeme
- Koken: BinaryData resource adi yanlis anahtarla okunuyordu.
- Fix: resource key adlari `*_jpg`, `*_png` formatinda dogru kullanildi.

2. Reverse "inputu ters ceviriyor" hissi
- Koken: reverse read kaynagi wet yerine ana delay buffer'di.
- Fix: ayri reverse wet-history buffer.

3. Eski preset + yeni parametre uyumu
- Dogrulama: eski XML preset yeni parametreyi icermese de APVTS default degerde kalir.
- Sonuc: crash yok, yeni parametre default ile devam eder.

## 12) PHASEUS Signature (Lock)

- Full dark, high-contrast, compact header.
- Reusable preset bar + centered placement.
- Square button geometry (radius 0).
- Sag panelde overflow-safe scroll.
- Mode-specific clarity + wet path safety (filter/diffusion/reverse mantigi wet uzerinde).
