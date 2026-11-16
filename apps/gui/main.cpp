// Akıllı Hibrit Enerji Asistanı — grafik arayüz.
// Bu dosya yalnızca pencereyi (GLFW), OpenGL'i ve ImGui/ImPlot'u kurar;
// arayüzün kendisi App sınıfındadır.
//
// Komut satırı (hepsi isteğe bağlı):
//   --cevrim <ad>             Açılışta seçili hazır çevrim (ör. nedc)
//   --strateji <ad>           ice | ev | kural | model
//   --cevrim-dosyasi <csv>    CSV çevrimi ekle ve seç
//   --ayar-dosyasi <ini>      Ayar dosyasını yükle
//   --kayit-klasoru <klasör>  "CSV olarak kaydet" / "Görüntü kaydet" dosya penceresi
//                             açmadan bu klasöre önerilen adla yazar
//   --ekran-goruntusu <png>   Pencereyi göstermeden arayüzü çiz, PNG'ye yaz ve çık
//   --boyut <G>x<Y>           Pencere boyutu (varsayılan 1400x860)
//   --olcek <k>               Yazı ve arayüz ölçeği (varsayılan: ekranın DPI ölçeği)
//   --kare <n>                Ekran görüntüsünden önce çizilecek kare sayısı (varsayılan 5)
//   --eylem <eylem>           Yalnızca ekran görüntüsü modunda: görüntüden önce arayüzde
//                             fare/klavye olayı taklit et (birden çok kez verilebilir):
//                               tikla:<kontrol>            kontrolün ortasına tıkla
//                               kaydir:<kontrol>:<0..1>    kaydırıcıda o orana tıkla
//                               tus:F5 / tus:Space         F5 / Boşluk tuşuna bas
//                               bekle:<n>                  n kare çiz (canlı oynatma ilerler)
//                               fare:<x>:<y>               fareyi o piksele götür (tıklamadan)
//                             Kontrol adları: App::findWidget (app.h)
//
// Ekran görüntüsü modunda son durum stdout'a tek satır yazılır (App::statusLine).
// Bu modda her kare 1/60 s sayılır (gerçek saat kullanılmaz): canlı oynatma
// deterministik ilerler, testler her bilgisayarda aynı sonucu verir.
// Çıkış kodları: 0 başarılı, 2 hatalı argüman, 3 pencere/OpenGL hatası (normal modda
// hata kutusu da gösterilir), 4 arayüz hata gösteriyor (ör. çevrim bulunamadı),
// 5 PNG yazılamadı, 6 eylem uygulanamadı (ör. kontrol ekranda yok).
// Test için ortam değişkeni HEA_TEST_OPENGL_SURUMU=<M.m> istenen OpenGL sürümünü
// değiştirir (ör. 9.9: açılış hatası bilerek üretilir).

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX // windows.h'nin min/max makroları std::min ile çakışmasın
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "app.h"
#include "embedded_font.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include "platform.h"
#include "png_writer.h"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitUsage = 2;
constexpr int kExitGraphics = 3;
constexpr int kExitAppError = 4;
constexpr int kExitPng = 5;
constexpr int kExitAction = 6;

// Ekran görüntüsü modunda bir karenin sayılan süresi [s]
constexpr float kScreenshotFrameSeconds = 1.0f / 60.0f;

// ---- Kullanılan OpenGL işlevleri (GLFW ile yüklenir; ek yükleyici gerekmez) ----
#ifdef _WIN32
#define HEA_GLAPI __stdcall
#else
#define HEA_GLAPI
#endif
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLbitfield = unsigned int;
using GLfloat = float;

constexpr GLenum GL_COLOR_BUFFER_BIT_ = 0x00004000;
constexpr GLenum GL_RGBA_ = 0x1908;
constexpr GLenum GL_RGBA8_ = 0x8058;
constexpr GLenum GL_UNSIGNED_BYTE_ = 0x1401;
constexpr GLenum GL_PACK_ALIGNMENT_ = 0x0D05;
constexpr GLenum GL_FRAMEBUFFER_ = 0x8D40;
constexpr GLenum GL_RENDERBUFFER_ = 0x8D41;
constexpr GLenum GL_COLOR_ATTACHMENT0_ = 0x8CE0;
constexpr GLenum GL_FRAMEBUFFER_COMPLETE_ = 0x8CD5;

struct Gl {
    void(HEA_GLAPI* Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void(HEA_GLAPI* ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void(HEA_GLAPI* Clear)(GLbitfield) = nullptr;
    void(HEA_GLAPI* PixelStorei)(GLenum, GLint) = nullptr;
    void(HEA_GLAPI* ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;
    void(HEA_GLAPI* GenFramebuffers)(GLsizei, GLuint*) = nullptr;
    void(HEA_GLAPI* BindFramebuffer)(GLenum, GLuint) = nullptr;
    void(HEA_GLAPI* GenRenderbuffers)(GLsizei, GLuint*) = nullptr;
    void(HEA_GLAPI* BindRenderbuffer)(GLenum, GLuint) = nullptr;
    void(HEA_GLAPI* RenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei) = nullptr;
    void(HEA_GLAPI* FramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint) = nullptr;
    GLenum(HEA_GLAPI* CheckFramebufferStatus)(GLenum) = nullptr;
    void(HEA_GLAPI* DeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
    void(HEA_GLAPI* DeleteRenderbuffers)(GLsizei, const GLuint*) = nullptr;

    template <typename F>
    static bool load(F& f, const char* name)
    {
        f = reinterpret_cast<F>(glfwGetProcAddress(name));
        return f != nullptr;
    }

    bool loadBasic()
    {
        return load(Viewport, "glViewport") && load(ClearColor, "glClearColor")
               && load(Clear, "glClear");
    }

    bool loadOffscreen()
    {
        return load(PixelStorei, "glPixelStorei") && load(ReadPixels, "glReadPixels")
               && load(GenFramebuffers, "glGenFramebuffers")
               && load(BindFramebuffer, "glBindFramebuffer")
               && load(GenRenderbuffers, "glGenRenderbuffers")
               && load(BindRenderbuffer, "glBindRenderbuffer")
               && load(RenderbufferStorage, "glRenderbufferStorage")
               && load(FramebufferRenderbuffer, "glFramebufferRenderbuffer")
               && load(CheckFramebufferStatus, "glCheckFramebufferStatus")
               && load(DeleteFramebuffers, "glDeleteFramebuffers")
               && load(DeleteRenderbuffers, "glDeleteRenderbuffers");
    }
};

struct Args {
    gui::StartOptions start;
    std::string screenshotPath;
    int width = 1400;
    int height = 860;
    bool sizeGiven = false;   // --boyut verildi mi? (verilmediyse ekrana sığdırılır)
    float uiScale = 0.0f;     // --olcek: yazı/arayüz ölçeği (0 = ekranın DPI ölçeği)
    int frames = 5;
    std::vector<std::string> actions;
};

bool parseInt(const std::string& s, int& out)
{
    if (s.empty() || s.size() > 6) return false;
    int v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    return true;
}

// Hata varsa açıklama döndürür. Ekran görüntüsü istenip istenmediği hatalardan
// bağımsız olarak belirlenir: hatalı bir argüman yüzünden kullanıcının ekranında
// görünür pencere açılmamalı.
std::string parseArgsImpl(const std::vector<std::string>& a, Args& out);

std::string parseArgs(const std::vector<std::string>& a, Args& out)
{
    const std::string err = parseArgsImpl(a, out);
    if (!err.empty() && out.screenshotPath.empty()) {
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a[i] == "--ekran-goruntusu") {
                // Değer yoksa da ekran görüntüsü modu: program hemen hata koduyla çıkar
                out.screenshotPath = i + 1 < a.size() ? a[i + 1] : std::string("?");
                break;
            }
        }
    }
    return err;
}

std::string parseArgsImpl(const std::vector<std::string>& a, Args& out)
{
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto value = [&](std::string& dst) {
            if (i + 1 >= a.size()) return false;
            dst = a[++i];
            return true;
        };
        const std::string& k = a[i];
        std::string v;
        if (k == "--cevrim") {
            if (!value(out.start.cycle)) return k + " bir deger bekliyor";
        } else if (k == "--strateji") {
            if (!value(out.start.strategy)) return k + " bir deger bekliyor";
        } else if (k == "--kayit-klasoru") {
            if (!value(out.start.saveDir)) return k + " bir deger bekliyor";
        } else if (k == "--cevrim-dosyasi") {
            if (!value(out.start.cycleFile)) return k + " bir deger bekliyor";
        } else if (k == "--ayar-dosyasi") {
            if (!value(out.start.configFile)) return k + " bir deger bekliyor";
        } else if (k == "--ekran-goruntusu") {
            if (!value(out.screenshotPath)) return k + " bir deger bekliyor";
        } else if (k == "--olcek") {
            if (!value(v)) return k + " bir deger bekliyor";
            char* end = nullptr;
            out.uiScale = std::strtof(v.c_str(), &end);
            if (v.empty() || *end != 0 || !(out.uiScale >= 0.5f && out.uiScale <= 4.0f)) {
                return "--olcek 0.5..4 arasinda bir sayi olmali";
            }
        } else if (k == "--boyut") {
            out.sizeGiven = true;
            if (!value(v)) return k + " bir deger bekliyor";
            const auto x = v.find('x');
            if (x == std::string::npos || !parseInt(v.substr(0, x), out.width)
                || !parseInt(v.substr(x + 1), out.height) || out.width < 320 || out.height < 240
                || out.width > 8192 || out.height > 8192) {
                return "--boyut GxY biciminde olmali (ornek 1400x860, en az 320x240)";
            }
        } else if (k == "--eylem") {
            if (!value(v)) return k + " bir deger bekliyor";
            out.actions.push_back(v);
        } else if (k == "--kare") {
            if (!value(v) || !parseInt(v, out.frames) || out.frames < 1 || out.frames > 1000) {
                return "--kare 1..1000 arasinda bir tam sayi olmali";
            }
        } else {
            return "bilinmeyen secenek '" + k + "'";
        }
    }
    return {};
}

// Son GLFW hatası: grafik başlatılamazsa kullanıcıya nedeniyle birlikte gösterilir
std::string g_lastGlfwError;

void glfwError(int code, const char* description)
{
    std::fprintf(stderr, "GLFW hatasi %d: %s\n", code, description);
    g_lastGlfwError = description != nullptr ? description : "";
}

// Pencere ya da OpenGL 3 başlatılamadı. Program konsolsuz olduğu için stderr görünmez:
// normal modda kullanıcıya hata kutusu gösterilir, yoksa çift tıklayınca hiçbir şey
// olmamış gibi görünür. Ekran görüntüsü modunda kutu açılmaz (testler beklemesin).
int failGraphics(bool screenshot)
{
    std::fprintf(stderr, "Grafik baslatilamadi: OpenGL 3.0 destekli surucu gerekli\n");
    if (!screenshot) {
        gui::showErrorBox(
            "Akıllı Hibrit Enerji Asistanı",
            "Grafik arayüz başlatılamadı.\n\n"
            "Program, OpenGL 3.0 destekleyen bir ekran kartı sürücüsü gerektirir. "
            "Ekran kartı sürücüsünü güncelleyip yeniden deneyin. Uzak Masaüstü "
            "bağlantısında ya da sanal makinede de bu hata görülebilir.\n\n"
            "Terminal sürümü (hea_cli.exe) OpenGL gerektirmez.\n\n"
            "Ayrıntı: " + (g_lastGlfwError.empty() ? std::string("-") : g_lastGlfwError));
    }
    return kExitGraphics;
}

// Gömülü fontu Türkçe harflerle (Latin-1 + Latin Extended-A) yükler.
void loadFont(float size_px)
{
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // Temel Latin + Latin-1 (ç, ö, ü, Ç, Ö, Ü, °)
        0x0100, 0x017F, // Latin Extended-A (ğ, Ğ, ı, İ, ş, Ş)
        0,
    };
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false; // veri statik dizide; ImGui silmesin
    ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(gui::kFontData),
                                               gui::kFontSize, size_px, &cfg, ranges);
}

// Bir kare: ImGui'yi çiz ve o an bağlı çerçeve tamponuna işle.
// fixedDelta > 0 ise kare süresi gerçek saat yerine bu değerdir.
void renderFrame(const Gl& gl, gui::App& app, int fbWidth, int fbHeight, float fixedDelta = 0.0f)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    if (fixedDelta > 0.0f) {
        ImGui::GetIO().DeltaTime = fixedDelta;
    }
    ImGui::NewFrame();
    app.draw();
    ImGui::Render();
    gl.Viewport(0, 0, fbWidth, fbHeight);
    gl.ClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    gl.Clear(GL_COLOR_BUFFER_BIT_);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void* nativeHandle(GLFWwindow* window)
{
#ifdef _WIN32
    return glfwGetWin32Window(window);
#else
    (void)window;
    return nullptr;
#endif
}

// Bir kullanıcı eylemini taklit eder; frames(n) n kare çizer. Hata varsa açıklama döner.
template <typename Frames>
std::string runAction(const gui::App& app, const std::string& spec, Frames&& frames)
{
    ImGuiIO& io = ImGui::GetIO();
    if (spec == "tus:F5" || spec == "tus:Space") {
        const ImGuiKey key = spec == "tus:F5" ? ImGuiKey_F5 : ImGuiKey_Space;
        io.AddKeyEvent(key, true);
        frames(1);
        io.AddKeyEvent(key, false);
        frames(2);
        return {};
    }
    if (spec.rfind("fare:", 0) == 0) {
        // Fareyi pencere içindeki bir piksele götür (tıklamadan): üzerine gelme durumları
        const std::string rest = spec.substr(5);
        const auto colon = rest.find(':');
        int x = 0, y = 0;
        if (colon == std::string::npos || !parseInt(rest.substr(0, colon), x)
            || !parseInt(rest.substr(colon + 1), y)) {
            return "fare:<x>:<y> bekleniyordu (piksel)";
        }
        io.AddMousePosEvent(static_cast<float>(x), static_cast<float>(y));
        frames(2);
        return {};
    }
    if (spec.rfind("bekle:", 0) == 0) {
        int n = 0;
        if (!parseInt(spec.substr(6), n) || n < 1 || n > 100000) {
            return "bekle:<kare> 1..100000 arasinda olmali";
        }
        frames(n);
        return {};
    }
    std::string key;
    double frac = 0.5;
    if (spec.rfind("tikla:", 0) == 0) {
        key = spec.substr(6);
    } else if (spec.rfind("kaydir:", 0) == 0) {
        const std::string rest = spec.substr(7);
        const auto colon = rest.rfind(':');
        if (colon == std::string::npos) return "kaydir:<kontrol>:<0..1> bekleniyordu";
        key = rest.substr(0, colon);
        const std::string num = rest.substr(colon + 1);
        char* end = nullptr;
        frac = std::strtod(num.c_str(), &end);
        if (num.empty() || *end != '\0' || !(frac >= 0.0 && frac <= 1.0)) {
            return "oran 0..1 arasinda olmali";
        }
    } else {
        return "bilinmeyen eylem (tikla:, kaydir:, tus:F5, tus:Space, bekle:, fare:)";
    }
    gui::App::Rect r;
    if (!app.findWidget(key, r)) return "kontrol ekranda yok: " + key;
    // Kenarın 1 piksel içi: oran 0 ve 1 de kontrolün üstünde kalsın
    const float x = r.x0 + 1.0f + (r.x1 - r.x0 - 2.0f) * static_cast<float>(frac);
    const float y = (r.y0 + r.y1) * 0.5f;
    io.AddMousePosEvent(x, y);
    frames(1);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frames(1);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frames(2);
    return {};
}

// O an bağlı çerçeve tamponunu (görünür pencerede arka tampon, ekran görüntüsü modunda
// ekran dışı tampon) okuyup PNG'ye yazar. utf8Path Türkçe karakter içerebilir.
bool captureToPng(const Gl& gl, int w, int h, const std::string& utf8Path)
{
    if (gl.ReadPixels == nullptr || gl.PixelStorei == nullptr || w <= 0 || h <= 0) return false;
    std::vector<unsigned char> rgba(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4);
    gl.PixelStorei(GL_PACK_ALIGNMENT_, 1);
    gl.ReadPixels(0, 0, w, h, GL_RGBA_, GL_UNSIGNED_BYTE_, rgba.data());

    // OpenGL alt satırdan başlar; PNG üst satırdan
    std::vector<unsigned char> rgb(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3);
    for (int y = 0; y < h; ++y) {
        const unsigned char* src = rgba.data() + static_cast<std::size_t>(h - 1 - y) * static_cast<std::size_t>(w) * 4;
        unsigned char* dst = rgb.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 3;
        for (int x = 0; x < w; ++x) {
            dst[x * 3 + 0] = src[x * 4 + 0];
            dst[x * 3 + 1] = src[x * 4 + 1];
            dst[x * 3 + 2] = src[x * 4 + 2];
        }
    }
    return gui::writePngRgb(utf8Path, w, h, rgb);
}

// Arayüz bu karede "Görüntü kaydet" istediyse: fare pencerenin dışındaymış gibi bir kare
// daha çizilir (tıklanan düğmenin vurgusu ve ipucu görüntüye girmesin), o kare PNG'ye
// yazılır, fare konumu geri verilir. render(): bir kare çizer (ekrana vermeden).
template <typename Render>
void serveScreenshotRequest(const Gl& gl, gui::App& app, int w, int h, Render&& render)
{
    std::string path;
    if (!app.takeScreenshotRequest(path)) return;
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mouse = io.MousePos;
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    render();
    app.screenshotSaved(path, captureToPng(gl, w, h, path));
    io.AddMousePosEvent(mouse.x, mouse.y);
}

// Görünmez pencerede birkaç kare çizip son kareyi PNG'ye yazar.
int runScreenshot(const Gl& glBasic, GLFWwindow* window, gui::App& app, const Args& args)
{
    Gl gl = glBasic;
    if (!gl.loadOffscreen()) {
        std::fprintf(stderr, "OpenGL framebuffer islevleri yuklenemedi\n");
        return kExitGraphics;
    }
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    if (w <= 0 || h <= 0) return kExitGraphics;

    // Ekran dışı çerçeve tamponu: gizli pencerenin kendi tamponu her sürücüde
    // okunabilir olmayabilir
    GLuint fbo = 0, rbo = 0;
    gl.GenFramebuffers(1, &fbo);
    gl.BindFramebuffer(GL_FRAMEBUFFER_, fbo);
    gl.GenRenderbuffers(1, &rbo);
    gl.BindRenderbuffer(GL_RENDERBUFFER_, rbo);
    gl.RenderbufferStorage(GL_RENDERBUFFER_, GL_RGBA8_, w, h);
    gl.FramebufferRenderbuffer(GL_FRAMEBUFFER_, GL_COLOR_ATTACHMENT0_, GL_RENDERBUFFER_, rbo);
    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER_) != GL_FRAMEBUFFER_COMPLETE_) {
        std::fprintf(stderr, "Ekran disi cerceve tamponu olusturulamadi\n");
        return kExitGraphics;
    }

    auto frames = [&](int n) {
        for (int i = 0; i < n; ++i) {
            glfwPollEvents();
            gl.BindFramebuffer(GL_FRAMEBUFFER_, fbo);
            renderFrame(gl, app, w, h, kScreenshotFrameSeconds);
            serveScreenshotRequest(gl, app, w, h, [&] {
                gl.BindFramebuffer(GL_FRAMEBUFFER_, fbo);
                renderFrame(gl, app, w, h, kScreenshotFrameSeconds);
            });
        }
    };
    // İlk karelerde ImGui yerleşimi ve grafik eksenleri oturur
    frames(args.frames);

    // Taklit edilen kullanıcı eylemleri (gerçek fare/klavye olayları gibi ImGui'ye verilir)
    for (const std::string& action : args.actions) {
        const std::string err = runAction(app, action, frames);
        if (!err.empty()) {
            std::fprintf(stderr, "Eylem '%s': %s\n", action.c_str(), err.c_str());
            gl.BindFramebuffer(GL_FRAMEBUFFER_, 0);
            gl.DeleteRenderbuffers(1, &rbo);
            gl.DeleteFramebuffers(1, &fbo);
            return kExitAction;
        }
    }
    if (!args.actions.empty()) {
        frames(args.frames);
    }
    std::printf("%s\n", app.statusLine().c_str());
    std::fflush(stdout);

    gl.BindFramebuffer(GL_FRAMEBUFFER_, fbo);
    const bool pngOk = captureToPng(gl, w, h, args.screenshotPath);
    gl.BindFramebuffer(GL_FRAMEBUFFER_, 0);
    gl.DeleteRenderbuffers(1, &rbo);
    gl.DeleteFramebuffers(1, &fbo);
    if (!pngOk) {
        std::fprintf(stderr, "PNG yazilamadi: %s\n", args.screenshotPath.c_str());
        return kExitPng;
    }
    if (!app.lastError().empty()) {
        std::fprintf(stderr, "Arayuz hata gosteriyor: %s\n", app.lastError().c_str());
        return kExitAppError;
    }
    return kExitOk;
}

} // namespace

int main(int argc, char** argv)
{
    Args args;
    const std::string argError = parseArgs(gui::commandLineArgsUtf8(argc, argv), args);
    const bool screenshot = !args.screenshotPath.empty();
    if (!argError.empty() && screenshot) {
        std::fprintf(stderr, "Hata: %s\n", argError.c_str());
        return kExitUsage;
    }
    if (!argError.empty()) {
        args.start.error = "komut satiri: " + argError;
    }

    glfwSetErrorCallback(glfwError);
    if (!glfwInit()) {
        return failGraphics(screenshot);
    }
    // Yalnızca test için: istenen OpenGL sürümü değiştirilebilir (ör. "9.9" hiçbir
    // sürücüde yoktur: açılış hatası, hata kutusu ve çıkış kodu 3 böyle denenir)
    int glMajor = 3, glMinor = 0;
    if (const char* forced = std::getenv("HEA_TEST_OPENGL_SURUMU")) {
        int ma = 0, mi = 0;
        if (std::sscanf(forced, "%d.%d", &ma, &mi) == 2) {
            glMajor = ma;
            glMinor = mi;
        }
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, glMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, glMinor);
    // Pencere önce gizli oluşturulur; normal modda konumlandırıldıktan sonra gösterilir.
    // Ekran görüntüsü modunda hiç gösterilmez (kullanıcının ekranında pencere açılmaz).
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // Boyut verilmediyse pencere ekranın kullanılabilir alanına sığdırılır
    // (ör. 1366x768 dizüstü ekranında taşmasın) ve ortalanır
    int wx = 0, wy = 0, ww = 0, wh = 0;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor != nullptr) {
        glfwGetMonitorWorkarea(monitor, &wx, &wy, &ww, &wh);
    }
    // Yüksek DPI ekranda (ör. %150 ölçek) arayüz büyük çizilir: varsayılan pencere de
    // aynı oranda büyüsün, yoksa içerik sıkışır. Ardından ekrana sığdırılır.
    if (!screenshot && !args.sizeGiven && monitor != nullptr) {
        float sx = 1.0f, sy = 1.0f;
        glfwGetMonitorContentScale(monitor, &sx, &sy);
        if (sx > 1.0f && sy > 1.0f && sx <= 4.0f && sy <= 4.0f) {
            args.width = static_cast<int>(static_cast<float>(args.width) * sx);
            args.height = static_cast<int>(static_cast<float>(args.height) * sy);
        }
    }
    if (!screenshot && !args.sizeGiven && ww > 0 && wh > 0) {
        args.width = std::min(args.width, ww * 92 / 100);
        args.height = std::min(args.height, wh * 88 / 100); // başlık çubuğuna yer kalsın
    }
    GLFWwindow* window = glfwCreateWindow(args.width, args.height,
                                          "Akıllı Hibrit Enerji Asistanı", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return failGraphics(screenshot);
    }
    if (!screenshot) {
        if (ww > 0 && wh > 0) {
            glfwSetWindowPos(window, wx + (ww - args.width) / 2, wy + (wh - args.height) / 2);
        }
        glfwShowWindow(window);
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // dikey eşitleme: ekran tazeleme hızında çiz

    Gl gl;
    if (!gl.loadBasic()) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return failGraphics(screenshot);
    }
    if (!screenshot) {
        // "Görüntü kaydet" için piksel okuma işlevleri. Yüklenemezse program yine çalışır,
        // yalnızca görüntü kaydı hata verir (captureToPng boş işlevleri denetler).
        gl.loadOffscreen();
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // yanına imgui.ini dosyası yazmasın

    // Yüksek DPI ekranlarda yazılar küçük kalmasın
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    if (args.uiScale > 0.0f) xscale = args.uiScale;
    loadFont(17.0f * xscale); // Fonts[0]: varsayılan
    loadFont(26.0f * xscale); // Fonts[1]: ana sonuç rakamları
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(xscale);
    // Açılır listeler opak olsun (alttaki yazılar karışmasın)
    ImGui::GetStyle().Colors[ImGuiCol_PopupBg].w = 1.0f;
    // Eksenler veriye oturtulurken üst/altta pay bırak (tepe noktası kenara yapışmasın)
    ImPlot::GetStyle().FitPadding = ImVec2(0.0f, 0.08f);

    // Ekran görüntüsü modunda işletim sisteminin gerçek fare/klavye olayları bağlanmaz:
    // yalnızca --eylem ile taklit edilenler etkili olur
    ImGui_ImplGlfw_InitForOpenGL(window, !screenshot);
    ImGui_ImplOpenGL3_Init("#version 130");

    int exitCode = kExitOk;
    {
        gui::App app(nativeHandle(window), args.start);
        if (screenshot) {
            exitCode = runScreenshot(gl, window, app, args);
        } else {
            while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();
                int w = 0, h = 0;
                glfwGetFramebufferSize(window, &w, &h);
                if (w > 0 && h > 0) { // simge durumundayken çizme
                    renderFrame(gl, app, w, h);
                    // Görüntü arka tampondan, ekrana vermeden önce alınır
                    serveScreenshotRequest(gl, app, w, h, [&] { renderFrame(gl, app, w, h); });
                    glfwSwapBuffers(window);
                } else {
                    glfwWaitEvents();
                }
            }
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}
