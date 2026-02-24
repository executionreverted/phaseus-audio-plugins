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
  - Dark mode toggle.
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

