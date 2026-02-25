# VST Library Guide (Agent Memory)

Bu dosya, bu repoda ileride hızlı context almak ve tekrarlanan hataları azaltmak icin yazildi.

## 1. Proje Amaci ve Mimari

- Bu repo bir JUCE plugin monorepo.
- Tek ortak JUCE klasoru kullanilir: `JUCE/`
- Her plugin ayri klasorde tutulur: `Plugins/<PluginName>/`
- Yeni plugin olusturma standart yolu: `./createplugin <PluginName>`
- Template kaynagi: `Templates/DefaultPluginTemplate/`

Temel dizin:

```text
VST_Library/
  CMakeLists.txt
  JUCE/
  Plugins/
    CMakeLists.txt
    <PluginName>/
      CMakeLists.txt
      Source/
  Templates/
    DefaultPluginTemplate/
  createplugin
```

## 2. Build Sistemi ve Akis

- Root `CMakeLists.txt`:
  - JUCE klasoru varsa `add_subdirectory(JUCE)` yapar.
  - Sonra `add_subdirectory(Plugins)` cagirir.
- `Plugins/CMakeLists.txt`:
  - Alt klasorleri otomatik tarar.
  - Icinde `CMakeLists.txt` olan klasorleri plugin olarak build'e dahil eder.
- Bu nedenle yeni plugin icin merkezi CMake dosyasina manuel satir eklenmez.

Standart komutlar:

```bash
git clone https://github.com/juce-framework/JUCE.git JUCE
cmake -S . -B build
cmake --build build --config Release
```

## 3. Yeni Plugin Olusturma Standardi

- Komut: `./createplugin Reverb2`
- Script:
  - Template'i `Plugins/Reverb2/` altina kopyalar.
  - Placeholder'lari doldurur:
    - `__PLUGIN_NAME__`
    - `__PLUGIN_CLASS__`
    - `__PLUGIN_CODE__`
- Uretim sonrasi plugin otomatik olarak build'e dahil olur (auto-discovery).

## 4. Bu Repoda Yapilacak Islerde Kalici Kurallar

- Yeni plugin acarken her zaman `createplugin` kullan.
- Elle klasor acip copy-paste ile plugin olusturma (drift olusur).
- CMake degisikligi gerekmedikce `Plugins/CMakeLists.txt` dosyasina dokunma.
- Ortak davranis/template degisimi gerekiyorsa once `Templates/DefaultPluginTemplate/` guncelle.
- Plugin ozel isler plugin klasorunde kalmali, root'u kirletme.

## 5. Common Mistakes (Tekrar Etme)

### Repo/Build duzeyi

- Hata: `JUCE/` olmadan build denemek.
  - Cozum: once JUCE'yi root'a clone et.
- Hata: Yeni plugin ekleyince `Plugins/CMakeLists.txt` elle edit etmek.
  - Cozum: auto-discovery zaten var, gerekmez.
- Hata: Gecerli olmayan plugin ismi (`-`, bosluk, sayi ile baslama).
  - Cozum: `^[A-Za-z][A-Za-z0-9_]*$` formatini kullan.

### JUCE/VST duzeyi

- Hata: Audio thread icinde memory allocation yapmak.
  - Cozum: `processBlock` icinde `new`, `delete`, `std::vector::push_back` gibi islemlerden kac.
- Hata: Audio thread'de lock kullanmak (`std::mutex`, file I/O, logging).
  - Cozum: lock-free veya preallocated yapi kullan.
- Hata: Parametreyi smoothing olmadan aniden uygulamak (zipper noise).
  - Cozum: `juce::SmoothedValue` veya kendi smoothing stratejini kullan.
- Hata: Kanal/layout temizligini ihmal etmek.
  - Cozum: kullanilmayan output channel'larini clear et; mono/stereo layout kontrolu yap.
- Hata: Denormal problemi.
  - Cozum: `juce::ScopedNoDenormals` kullan.
- Hata: State serialization unutmak.
  - Cozum: APVTS state'ini `getStateInformation/setStateInformation` ile sakla/yukle.
- Hata: A/B hostlarda farkli sample-rate/block-size davranisini test etmemek.
  - Cozum: farkli buffer ve sample-rate senaryolarinda smoke test yap.

## 6. Audio Processing Best Practices (Future Self Checklist)

- Realtime-safety:
  - Audio callback deterministic ve non-blocking olmali.
  - Heap allocation, filesystem, network, lock yok.
- Parameter management:
  - Tum host-automatable parametreler APVTS ile tanimlanmali.
  - Parametre ID'leri kalici olmali; sonradan key degistirme migration ister.
  - Kritik parametrelerde smoothing kullan.
- DSP kalite:
  - Headroom dusun (clipping'i kontrol et).
  - Wet/dry mix'te gain compensation ihtiyacini degerlendir.
  - Delay/reverb modullerinde tail ve bypass davranisini net tanimla.
- Numerics:
  - Float guvenli sinirlama (`jlimit`, clamp) yap.
  - Invalid state'te sessiz fallback uygula (nan/inf korumasi).
- UX:
  - Parametre adlari net ve muziksel olmali.
  - Default degerler kullanilabilir bir ses vermeli.
  - Editor olmasa bile DSP stabil calismali.
- Portability:
  - Mac/Windows host farklarini varsayma.
  - VST3/AU formatlarinda minimum smoke test yap.

## 7. Test ve Dogrulama Rutini

Her plugin degisiminden sonra minimum:

1. Proje configure/build:
   - `cmake -S . -B build`
   - `cmake --build build --config Release`
2. Plugin load smoke test (host icinde):
   - Plugin aciliyor mu?
   - Parametreler host automation'a cikiyor mu?
   - Audio drop/click var mi?
3. Preset/state test:
   - Kaydet/yukle parametreler geri geliyor mu?

## 8. Agent Icin Operasyon Notlari

- Bu dosya "hafiza" olarak okunmali; yeni gorevde once buraya bak.
- Projede tekrar eden islerde once standart script/template kullan.
- Kullanici "hizli yeni plugin" isterse direkt `createplugin` akisini takip et.
- Buyuk refactor oncesi mevcut template ve create akisini bozmamaya dikkat et.

## 9. Definition of Done (Bu Repo icin)

- Yeni plugin klasoru olustu.
- Build sistemi plugin'i otomatik goruyor.
- Derleme hatasiz.
- APVTS state + temel editor/processor baglantisi calisiyor.
- Realtime-safety kurallari ihlal edilmiyor.

## 10. Ek Hafiza

- Proje ilerleme notlari, bug-fix kayitlari ve PHASEUS signature kararlarinin detayli kaydi:
  - `Plugins/PHASEUS_Delay/PROJECT_MEMORY.md`

## 11. Son Sprintten Kritik Dersler

1. Time randomizasyonu "surekli jitter" olmamali
- Problem: `time random` her block/sample degisirse delay read point surekli oynar ve noise/pitch warble olusur.
- Dogru yaklasim: random degeri bir echo dongusu boyunca "held" tut, sadece periyot sonunda yenile.

2. Sync modda time knob kaybolmamali
- Problem: Sync acilinca kullanici time knob ile muziksel kontrol hissini kaybediyor.
- Dogru yaklasim: Sync acikken knob gorunur kalir, text `1/4, 1/8T...` olur, knob division'i surer.

3. Sag panel overflow'u sadece responsive layout ile cozulmez
- Problem: Ozellikle grain/filter ek kontrollerinde dikey tasma olur.
- Dogru yaklasim: Gercek scrollbar + wheel scroll + kompakt spacing.

4. Filter UI'de cok knob, hizli UX'i bozar
- Problem: 5-6 mini knob "prototip" hissi verir.
- Dogru yaklasim: type tabs + response graph + horizontal sliderlar; Comb ekstra kontrolleri kosullu acilir.

5. Stereo pan davranisi sabit-guc olmali
- Problem: Basit linear pan enerji dengesini bozar, ani ses degisimi yaratir.
- Dogru yaklasim: constant-power pan (`sqrt` tabanli L/R gain).

## 12. Guncel PHASEUS Durumu (2026-02)

- Header:
  - Sol: `Phaseus Dayi Delay`
  - Orta: reusable preset bar (`Init`, preset list, `Save`, `Init`)
  - Sag: `Output Gain` (`-24..+24 dB`, default `0 dB`) + `LoFi` toggle
- Tema:
  - Full dark only (dark/light toggle kaldirildi)
  - Background image + dark overlay
  - Square buttons (radius 0) standardi
- Preset:
  - Save dialog dark-themed ve centered spawn
  - Secili preset property APVTS state icinde tutuluyor
  - Eski presetlerde yeni parametre yoksa default deger kullanilir (crash olmadan)

## 13. Yeni DSP Ozellikleri (Kalin Hafiza)

- Reverse:
  - Reverse artik dry/input degil wet history'den okunuyor
  - `Reverse Start Offset` + `Reverse End Offset` aktif
  - `start + end` reverse penceresini clamp'li sekilde daraltir
- Diffusion:
  - Wet path'e 2-stage allpass diffusion eklendi
  - Parametreler:
    - `Diffusion Enable`
    - `Diffusion Amount`
    - `Diffusion Size (ms)`
- Output stage:
  - Final outputa global `Output Gain` uygulanir

## 14. Reusable UI Kurallari (Diger Pluginler Icin)

- Preset bar:
  - `Shared/PhaseusUI/PhaseusPresetBar` kullan
  - Header ortasina yerlestir
- Button styling:
  - `TextButton` ve preset action button'larda square style (radius 0)
- Right panel:
  - Overflow olasiligi varsa scrollbar default acik tasarla
- Mod bazli layout:
  - Ana knoblar kaybolmayacak sekilde state gecisleri yap

## 15. Son Iterasyon Ozeti (2026-02, Layout + Routing)

- Delay panel yerlesimi:
  - Sol ust: delay controls
  - Sol alt: sabit filter panel
  - Sag panel: global controls
- Random knob yerlesimi:
  - Tum random parametrelerde random knob ana knobun saginda.
  - Alt alta random layout kaldirildi.
- Knob sizing:
  - Autoscale tamamen kaldirildi.
  - Sabit ve okunakli standart boyut kullaniliyor.
- Filter routing (hard rule):
  - `Filter 1` input daima wet delay sinyali
  - `Filter 2` enabled ise `F1 -> F2 -> Out`
  - `Filter 2` disabled ise `F1 -> Out`
- PingPong chain icon visibility:
  - Link toggle kapaliysa zincir ikonu gorunmez.

## 16. CI ve Tek Plugin Build Notu

- Monorepo oldugu icin tum pluginleri build etmek zorunlu degil.
- Lokal derlemede hedef vererek sadece tek plugin alinabilir:
  - `cmake --build build --config Release --target PHASEUS_Delay_VST3`
- GitHub Actions release akisinda da ayni yaklasim kullanilabilir:
  - `workflow_dispatch` input (`plugin_name`) eklenir
  - target dinamik secilir: `${plugin_name}_VST3`
  - sadece secili plugin ziplenip release'e eklenir.
