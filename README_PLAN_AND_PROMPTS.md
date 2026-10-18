# Drum Slicer (VST3 / Windows / Cubase) – برنامه، ساخت، پرامپت‌ها

## ۱) کار پلاگین چیه
1. فایل لوپ درام (WAV/MP3/FLAC) رو روی پنجره پلاگین Drag & Drop می‌کنی.
2. تشخیص ضربه‌ها (Spectral-flux onset) و طبقه‌بندی: Kick / Snare / Clap / C-Hat / O-Hat / Tom / Perc / Crash.
3. از هر دسته «تمیزترین» ضربه انتخاب و **خشک** برش داده می‌شه (فید ورودی ۱.۵ms، فید خروجی، تریم دم -۵۰dB، نرمالایز -۳dB). ضربه‌ای که نزدیک ضربه بعدی یا با دم ضربه قبلی قاطی باشه امتیاز کم می‌گیره.
4. دکمه **DRAG MIDI** → یه فایل میدی از همه ضربه‌ها (ولوسیتی نسبت به وان‌شات انتخابی، کوانتایز ۱/۱۶ قابل تنظیم) می‌سازه؛ بکشش روی ترک Cubase.
5. پلاگین با همون میدی، با وان‌شات‌های تمیز لوپ رو بازسازی و پخش می‌کنه (Choke هت‌ها، Decay/Pan/Vol هر پد، ۸ خروجی جدا برای میکس).
6. **EXPORT WAVS** وان‌شات‌ها رو ۲۴بیت در `Documents\DrumSlicer\<نام لوپ>\` می‌ریزه. **DRAG PAD WAV** پد انتخابی رو مستقیم می‌کشی توی Cubase.

نت‌ها (Cubase: C1=36): Kick 36، Snare 38، Clap 39، C-Hat 42، O-Hat 46، Tom 45، Perc 37، Crash 49.

## ۲) بهترین روش استخراج وان‌شات تمیز (و محدودیتش)
- روش V1 (داخل پلاگین): انتخاب ضربه ایزوله‌ترین نمونه از خود لوپ. برای درام Suno عالیه چون ضربه‌ها معمولاً جدا و تکرارشونده‌ان.
- حقیقت مهم: اگه توی لوپ هیچ ضربه‌ی ایزوله‌ای از یه ساز نباشه (مثلاً کیک همیشه با هت یا کراش همزمانه)، وان‌شاتِ ۱۰۰٪ خشک ممکن نیست؛ خروجی فقط «تمیزترین موجود» می‌شه.
- بهترین ورودی: از Suno حتماً **Stems → Drums** رو WAV بگیر (نه میکس کامل). فایل MP3 هم باز می‌شه ولی WAV بهتره.
- فاز ۲ (اگه V1 کافی نبود): جداسازی پیش‌پردازشی قبل از پلاگین با مدل DrumSep (MDX23C) یا Demucs → کیک/اسنر/هت/تام جدا، بعد همون پلاگین. این یه مرحله آفلاین پایتونی جدا می‌شه، توی پلاگین نمی‌ره.
- دسته‌بندی ساز با قانون‌های ساده (فرکانس/دیکی) انجام می‌شه؛ ممکنه گاهی کلپ/اسنر یا تام/کیک رو اشتباه بزنه. آستانه‌ها بالای `Extractor.h` تابع `classify` هستن. SENS رو برای ضربه‌های بیشتر/کمتر تنظیم کن (بعد از رها کردن دسته، دوباره آنالیز می‌شه).
- BPM حدسیه (از طول لوپ به بار گرد می‌شه). اگه اشتباه بود دستی بزن؛ میدی با BPM همون لحظه ساخته می‌شه. فرض: لوپ از ابتدای بار شروع می‌شه.

## ۳) ساخت (Windows)
1. نصب: Visual Studio 2022 (Desktop development with C++) + CMake + Git.
2. توی پوشه پروژه:
   ```
   cmake -B build -A x64
   cmake --build build --config Release
   ```
   JUCE خودکار دانلود می‌شه (نیاز به اینترنت). خروجی VST3 به `C:\Program Files\Common Files\VST3` کپی می‌شه (VS رو **Run as administrator** بزن، وگرنه دستی کپی کن از `build\DrumSlicer_artefacts\Release\VST3`).
3. Cubase: Studio > VST Plug-in Manager > Update. پلاگین رو به‌صورت Instrument Track لود کن.
4. کار با Cubase: لوپ رو بنداز روی پلاگین → DRAG MIDI رو بکش روی ترک همون Instrument → پلی.
5. ذخیره پروژه: مسیر لوپ توی پروژه ذخیره می‌شه و موقع باز کردن دوباره آنالیز می‌شه (فایل لوپ رو جابه‌جا نکن؛ یا WAVهای Export شده رو نگه دار).

اسکین PNG اختیاریه: پوشه `Assets` کنار CMakeLists بساز، فایل‌ها رو با همین نام‌ها بذار، دوباره CMake رو اجرا کن. بدون فایل، پلاگین اسکین فلز/چوب رو با کد می‌کشه و کار می‌کنه.

## ۴) نقشه چیدمان UI (۱۰۰۰×۶۰۰)
```
|چوب 64| [ صفحه نمایش شیشه‌ای: موج لوپ + خطوط رنگی ضربه‌ها  808x170 ] |چوب 64|
|      | [LOAD LOOP][DRAG MIDI >][DRAG PAD WAV >][EXPORT WAVS]          |      |
|      | پد ۸تایی ۴×۲ (۱۲۸px)         |  نام پد + نت                    |      |
|      | KICK  SNARE  CLAP  C-HAT     |  VOL   DECAY   PAN (پد انتخابی) |      |
|      | O-HAT TOM    PERC  CRASH     |  GLOBAL: MASTER SENS QUANT BPM  |      |
|      |        لوگو DRUM SLICER                                         |      |
```
فایل‌های PNG: ابعاد @2x گفته شده توی پرامپت دوم.

## ۵) پرامپت ChatGPT شماره ۱ – طراحی ظاهری (مرجع تصویری)
```
Design a hyper-realistic product render of a luxury hardware drum machine, shown as a flat front-on plugin UI, 1000x600 px aspect ratio (5:3), no perspective, orthographic top-down view.
Style: high-end boutique studio gear (think Teenage Engineering meets Neve / Moog craftsmanship), premium and clean, not cluttered.
- Faceplate: glossy brushed aluminum, horizontal brushing, subtle specular highlights, thin beveled edges, four small recessed Phillips screws in the corners.
- Left and right side cheeks: polished walnut wood panels, vertical grain, glossy lacquer with soft reflections, 64 px wide each, slightly beveled edge touching the metal.
- Top center: a recessed black glass display (about 808x170 px) with an amber glowing waveform and thin colored vertical hit markers, subtle glass reflection.
- Below the display: a row of 4 small rectangular buttons (dark graphite, amber text): LOAD LOOP, DRAG MIDI, DRAG PAD WAV, EXPORT WAVS.
- Bottom-left: a 4x2 grid of 8 large square rubber drum pads (128 px each) with dark graphite rubber, thin metal bezel, a tiny amber LED strip at top of each pad, labels KICK, SNARE, CLAP, C-HAT, O-HAT, TOM, PERC, CRASH engraved in white.
- Right of the pads: 3 large knurled aluminum knobs (VOL, DECAY, PAN) with amber value arcs; below them a section "GLOBAL" with 4 smaller knobs (MASTER, SENS, QUANT, BPM).
- Bottom center: engraved logo "DRUM SLICER" with tiny subtitle "LOOP > ONE-SHOTS > MIDI".
Color palette: silver/aluminum, walnut brown, graphite black, warm amber (#FFB347) accents. Soft realistic studio lighting, shallow reflections, 4K detail, no people, no background, no extra text.
```

## ۶) پرامپت ChatGPT شماره ۲ – فایل‌های اسکین (برای ساخت)
(جدا جدا بگیر، هر تصویر یه درخواست. اسم‌ها دقیقاً همین باشه، داخل `Assets/` بریز.)
```
I need production texture assets for a plugin UI skin. Generate each as a separate PNG, exact pixel size, no text, no UI controls, no mockup, no watermark, flat front-on view, seamless-looking and clean.

1) faceplate.png - 1744x1200 px. Glossy brushed aluminum plate, horizontal brushing, soft vertical light gradient (lighter at top), thin beveled edge, four small recessed screws in the corners (14 px from each corner at 1x scale). Completely empty surface, nothing else.

2) wood_left.png - 128x1200 px. Polished walnut wood panel, vertical grain, glossy lacquer, soft highlight on the left edge, slightly darker and beveled on the right edge (the edge that touches the metal faceplate).

3) wood_right.png - 128x1200 px. Same walnut panel mirrored: bevel on the left edge, highlight on the right edge.

4) knob.png - 256x256 px, TRANSPARENT background. Top-down view of a knurled brushed-aluminum rotary knob, round, with a thin black indicator line pointing straight UP from the center to the rim. Subtle soft drop shadow inside the canvas. Centered, symmetrical.

5) pad_off.png - 256x256 px, TRANSPARENT background. Top-down view of a square drum pad with rounded corners: dark graphite rubber surface, thin brushed-metal bezel, subtle texture. Unlit.

6) pad_on.png - 256x256 px, TRANSPARENT background. Identical to pad_off.png in shape and position but lit: warm amber (#FFB347) glow across the rubber surface and bezel edge. Must align pixel-perfectly with pad_off.png.
```
نکته: ChatGPT گاهی سایز دقیق نمی‌ده؛ در این صورت خودت توی Paint/Photopea رزایز کن (نسبت مهمه). پس‌زمینه شفاف رو حتماً چک کن (مربع‌های شطرنجی).

## ۷) آپدیت: تمیزترین و بی‌تداخل‌ترین وان‌شات
- امتیاز هر ضربه حالا شامل «خلوص باند» هم هست (کیک نباید های‌فرکانس زیاد داشته باشه، هت نباید باس داشته باشه، اسنر/کلپ نباید کیک تو خودش داشته باشه) + فاصله از ضربه بعدی + سکوت قبل از ضربه (بدون دم ضربه قبلی).
- برای هر پد ۶ کاندید نگه داشته می‌شه؛ با دکمه `<` و `>` بالای ولوم‌ها «TAKE» رو عوض می‌کنی و درصد CLEAN رو می‌بینی. ضربه انتخابی روی صفحه با نشانگر سفید معلوم می‌شه.
- CLEAN FILTER (پیش‌فرض روشن): فیلتر زیرفاز (بدون اسمیر شدن ترنزینت): هت/کراش بالای ۳۰۰Hz، کیک پایین ۹kHz، بقیه فقط رامبل زیر ۳۰Hz حذف می‌شه.
- «فیکس» یعنی همیشه: ترنزینت دقیقاً شروع، فید ثابت، تریم دم، نرمالایز -۳dB. ولوسیتی ۱۰۰ = همون وان‌شات بی‌تغییر.
- اگه باز هم تداخل بود، یعنی توی خود لوپ اون ساز هیچ‌جا تنها نیست؛ راه‌حل فقط جداسازی پیش‌پردازشی (DrumSep/Demucs) روی لوپ قبل از ورود به پلاگینه.

## ۸) نسخه ۲: موتور «درام‌پیل» – بازسازی ۱ به ۱ لوپ (بالای ۹۰٪)
فایل‌ها: `Source/DrumPeel.h` (موتور، سی‌پلاس‌پلاس خالص)، `Source/Extractor.h` (اتصال به جوس)، `tools/peel_cli.cpp` (نسخه خط فرمان همون موتور)، `tools/verify_match.py` (بررسی مستقل).

### روش (ایده‌ی بروس‌فری: یاد بگیر، کم کن، تکرار کن)
۱. ضربه‌ها پیدا می‌شن و به چند گروه صدا تقسیم می‌شن.
۲. برای هر صدا یه «الگوی هم‌فاز» از کل لوپ با هم حل می‌شه (کمترین مربعات، همه‌ی سازها هم‌زمان). یعنی کیکی که زیر اسنیره با فاز درست از اسنیر کم می‌شه، نه با فیلتر.
۳. قانون «درام فقط کم می‌شه»: دم هر صدا اجازه نداره دوباره بلند بشه، پس ضربه‌ی سازهای دیگه داخل وان‌شات نمی‌مونه.
۴. بخشی که هر بار فرق می‌کنه (هت، سیم اسنیر، هوای اتاق) با «پروفایل نویز» یاد گرفته می‌شه (ان‌ام‌اف روی طیف) و جدا ساخته می‌شه، طوری که با بخش هم‌فاز تداخل نکنه.
۵. هر ضربه زمان و ولوسیتی دقیق خودش رو می‌گیره؛ ضربه‌ای که با صدای خودش جور نیست به صدای درست منتقل می‌شه؛ هت روی کیک به‌صورت لایه اضافه می‌شه.
۶. صدایی که بدترین بازسازی رو داره دو نسخه می‌شه (مثلاً اسنیر و اسنیر ۲) تا ۸ پد پر بشه.
۷. خروجی: وان‌شات‌ها، میدی، و عدد تطابق که روی خود پخش پلاگین اندازه گرفته می‌شه.

### عدد تطابق یعنی چی
- «ریبیلد مچ»: سطح صدا در باندهای یک‌سوم اکتاو هر ۱۰ میلی‌ثانیه، جدا برای ساب/لو/لو-مید/مید/های. همون چیزی که گوش می‌شنوه. عدد اصلی = ضعیف‌ترین باند.
- «نال»: تست فاز به فاز شکل موج (مثل اینکه ریبیلد رو فاز برعکس روی اصلی بذاری).

نتیجه روی `drum_suno.wav`: ساب ۹۸.۳ · لو ۹۸.۲ · لو-مید ۹۰.۶ · مید ۹۰.۷ · های ۹۲.۹ · نال ۹۲.۴ درصد.

### برای اینکه توی کیوبیس همین عدد رو بگیری
- کوانتایز (QUANT) روی صفر بمونه. کوانتایز تطابق رو خراب می‌کنه.
- میدی با تمپوی پروژه ساخته می‌شه (پلاگین تمپوی هاست رو می‌خونه)، پس زمان ضربه‌ها به ثانیه درسته.
- میدی برای دقت زیر یک تیک، یک تیک قبل هر نت یه «پلی پرشر» داره. پاکش نکن.
- ولوم/دیکی/پن پدها روی پیش‌فرض باشن. هر ضربه کامل صدا می‌ده (دم قبلی قطع نمی‌شه، مثل لوپ اصلی).
- فایل `<loop>_rebuilt.wav` توی `Documents\DrumSlicer\<loop>\` هست؛ می‌تونی کنار اصلی بذاری و نال‌تست کنی.
- آنالیز یک بار انجام می‌شه و توی همون پوشه کش می‌شه؛ باز کردن دوباره‌ی پروژه فوری لود می‌شه. دکمه RE-ANALYSE دوباره آنالیز می‌کنه.

### بررسی مستقل
```
python tools/verify_match.py loop.wav "Documents/DrumSlicer/<loop>"
```
فایل‌های وو و میدی رو از دیسک می‌خونه، مثل پلاگین پخش می‌کنه و درصدها رو چاپ می‌کنه.

### محدودیت‌ها
- لوپ‌هایی که هر ضربه‌شون واقعاً فرق می‌کنه (هوش مصنوعی، اجرای زنده): «نال» پایین‌تر می‌آد (روی تست ساختگی حدود ۶۰٪)، ولی سطح شنیداری بالای ۹۰٪ موند.
- حداکثر ۸ صدا و ۶۰ ثانیه لوپ. آنالیز حدود ۲۰ تا ۶۰ ثانیه طول می‌کشه.

## ۹) نسخه ۳: حالت «درام ماشین»
هدف: از یه تیکه استم درام سونو، یه کیت تمیز و یکدست + میدی قابل ادیت، با وایب همون درام.

- **یه پد برای هر ساز**: نسخه‌های مختلف یه ساز (کیک، کیک ۲...) یکی می‌شن. روی لوپ تست: کیک، اسنیر، هت بسته.
- **آ/ب برای هر پد** (پایین سمت راست):
  - «آ: یادگرفته» = میانگین همه‌ی ضربه‌ها؛ خش استم و رنگ جداسازی که هر بار فرق می‌کنه، حذف می‌شه.
  - «ب: بهترین ضربه» = تمیزترین ضربه‌ی واقعی لوپ، سازهای دیگه با فاز درست ازش کم شدن، تا قبل از ضربه‌ی بعدی واقعیه و بعدش دم یادگرفته با کراس‌فید ادامه می‌ده.
  - روی هر دکمه بزنی همون نسخه پخش می‌شه؛ انتخاب توی پروژه ذخیره می‌شه.
- **تمیزکاری**: نویز کف استم از آرام‌ترین لحظه‌های لوپ یاد گرفته می‌شه و از دم وان‌شات‌ها پاک می‌شه (ترنزینت دست نمی‌خوره). هت پایین‌فرکانس نگه نمی‌داره.
- **دکمه‌ی میدی**:
  - «اگزکت» = کپی کامل الگو و گروو سونو (برای فیل‌ها و پترن‌های خاص).
  - «تایت» = روی گرید (۱/۱۶ یا تریوله، خودکار)، ولوسیتی در ۲ یا ۳ سطح برای هر ساز، نت‌های تکراری و ریزه‌کاری‌های آنالیز حذف. با تمپوی خود لوپ نوشته می‌شه، پس توی هر تمپویی مثل پترن درام‌ماشین پخش می‌شه.
  - دکمه‌ی «گرید»: فقط در حالت تایت؛ ۱ = کاملاً روی گرید، کمتر = کمی از گروو سونو.
- فایل‌ها در `Documents\DrumSlicer\<loop>\`: `_KICK.wav` (آ)، `_KICK_BEST.wav` (ب)، `_exact.mid` / `_tight.mid` (بعد از درگ میدی یا اکسپورت).

## ۱۰) نسخه ۴: ظاهر جدید + ادیتور صدا (BY POURIA MOTABEAN)
- ظاهر تیره و ساده با یه رنگ تأکید نارنجی؛ فایل‌های اسکین در `Assets/` (background, knob, pad_off, pad_on). نوشته‌ها و موج‌ها با کد کشیده می‌شن.
- ۸ پد در دو ردیف؛ هر ساز رنگ خودش رو داره (کیک نارنجی، اسنیر آبی، کلپ سبز، هت بسته فیروزه‌ای، هت باز بنفش...). روی هر پد کلید کوچیک آ/ب.
- **ادیتور پایین** (برای پد انتخاب‌شده):
  - پرچم سمت چپ = کات ابتدا (ضربه روی بیت می‌مونه، پلاگین خودش جبران می‌کنه)، خط سمت راست = کات انتها.
  - نقطه‌ها = اتک، هولد، دیکی (پوش ولوم). دوبار کلیک روی هر دستگیره = برگشت به پیش‌فرض. کلیک روی موج = شنیدن.
  - «درگ وو» = همون صدایی که می‌شنوی (با شیپ) رو بکش توی کیوبیس.
- پنل پایین راست: پانچ و سوستین (ترنزینت شیپر) و پیچ (نیم‌پرده) برای پد انتخاب‌شده.
- اکسپورت: وان‌شات‌های نهایی (نسخه‌ی انتخابی + شیپ) و میدی در `Documents\DrumSlicer\<loop>\EXPORT\`.
- همه‌ی تغییرات غیرمخربه و توی پروژه ذخیره می‌شه.
