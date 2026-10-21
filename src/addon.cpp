// SPDX-License-Identifier: MIT
// The UI runs in a separate process. Only this module touches input contexts.
#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx-config/enum.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/color.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/misc.h>
#include <fcitx-utils/trackableobject.h>
#include <fcitx-utils/utf8.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <string>
#include <vector>

FCITX_DEFINE_LOG_CATEGORY(kmojiLog, "kmoji");

namespace {
std::string hex(std::string_view s) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (unsigned char c : s) { out += digits[c >> 4]; out += digits[c & 15]; }
    return out;
}
std::string unhex(std::string_view s) {
    if (s.size() % 2 || s.size() > 16384) return {};
    std::string out;
    for (size_t i = 0; i < s.size(); i += 2) {
        unsigned value = 0;
        auto result = std::from_chars(s.data()+i, s.data()+i+2, value, 16);
        if (result.ec != std::errc{} || result.ptr != s.data()+i+2 || !value) return {};
        out += static_cast<char>(value);
    }
    return out;
}
std::vector<std::string> split(const std::string &s) {
    std::vector<std::string> out;
    size_t from = 0, at;
    while ((at = s.find('\t', from)) != std::string::npos) {
        out.push_back(s.substr(from, at-from)); from = at+1;
    }
    out.push_back(s.substr(from));
    return out;
}
}

FCITX_CONFIG_ENUM(UiLanguage, Auto, English, SimplifiedChinese);
struct UiLanguageI18n {
    bool skipDescription() const { return false; }
    bool skipSave() const { return false; }
    void dumpDescription(fcitx::RawConfig &config) const {
        config.setValueByPath("EnumI18n/0", fcitx::translateDomain("kmoji", "Auto"));
        config.setValueByPath("EnumI18n/1", fcitx::translateDomain("kmoji", "English"));
        config.setValueByPath("EnumI18n/2", fcitx::translateDomain("kmoji", "Simplified Chinese"));
    }
};
FCITX_CONFIG_ENUM(UiTheme, System, Light, Dark);
struct UiThemeI18n {
    bool skipDescription() const { return false; }
    bool skipSave() const { return false; }
    void dumpDescription(fcitx::RawConfig &config) const {
        config.setValueByPath("EnumI18n/0", fcitx::translateDomain("kmoji", "Follow system"));
        config.setValueByPath("EnumI18n/1", fcitx::translateDomain("kmoji", "Light"));
        config.setValueByPath("EnumI18n/2", fcitx::translateDomain("kmoji", "Dark"));
    }
};
FCITX_CONFIG_ENUM(HoverLanguage, English, Chinese);
struct HoverLanguageI18n {
    bool skipDescription() const { return false; }
    bool skipSave() const { return false; }
    void dumpDescription(fcitx::RawConfig &config) const {
        config.setValueByPath("EnumI18n/0", fcitx::translateDomain("kmoji", "English"));
        config.setValueByPath("EnumI18n/1", fcitx::translateDomain("kmoji", "Chinese"));
    }
};
FCITX_CONFIG_ENUM(SearchLanguage, Chinese, Pinyin);
struct SearchLanguageI18n {
    bool skipDescription() const { return false; }
    bool skipSave() const { return false; }
    void dumpDescription(fcitx::RawConfig &config) const {
        config.setValueByPath("EnumI18n/0", fcitx::translateDomain("kmoji", "Chinese"));
        config.setValueByPath("EnumI18n/1", fcitx::translateDomain("kmoji", "Pinyin"));
    }
};
using UiLanguageOption = fcitx::Option<UiLanguage, fcitx::NoConstrain<UiLanguage>,
    fcitx::DefaultMarshaller<UiLanguage>, UiLanguageI18n>;
using SearchLanguagesOption = fcitx::Option<std::vector<SearchLanguage>,
    fcitx::ListConstrain<fcitx::NoConstrain<SearchLanguage>>,
    fcitx::DefaultMarshaller<std::vector<SearchLanguage>>, SearchLanguageI18n>;
using HoverLanguagesOption = fcitx::Option<std::vector<HoverLanguage>,
    fcitx::ListConstrain<fcitx::NoConstrain<HoverLanguage>>,
    fcitx::DefaultMarshaller<std::vector<HoverLanguage>>, HoverLanguageI18n>;
using UiThemeOption = fcitx::Option<UiTheme, fcitx::NoConstrain<UiTheme>,
    fcitx::DefaultMarshaller<UiTheme>, UiThemeI18n>;
using AccentColorOption = fcitx::Option<fcitx::Color, fcitx::NoConstrain<fcitx::Color>,
    fcitx::DefaultMarshaller<fcitx::Color>, fcitx::NoAnnotation>;

FCITX_CONFIGURATION(KmojiConfig,
    fcitx::KeyListOption triggerKey{this, "TriggerKey",
        fcitx::translateDomain("kmoji", "Open emoji picker"),
        {fcitx::Key("Super+period"), fcitx::Key("Control+Alt+period")}, fcitx::KeyListConstrain()};
    UiThemeOption theme{this, "Theme", fcitx::translateDomain("kmoji", "Theme"), UiTheme::System};
    AccentColorOption accentColor{this, "AccentColor",
        fcitx::translateDomain("kmoji", "Accent color"), fcitx::Color("#7763d7")};
    UiLanguageOption language{this, "Language",
        fcitx::translateDomain("kmoji", "UI language"), UiLanguage::Auto};
    HoverLanguagesOption hoverLanguages{this, "HoverLanguages",
        fcitx::translateDomain("kmoji", "Hover languages (UI language always shown)"),
        {HoverLanguage::English, HoverLanguage::Chinese}};
    SearchLanguagesOption searchLanguages{this, "SearchLanguages",
        fcitx::translateDomain("kmoji", "Additional emoji search languages (English always on)"),
        {SearchLanguage::Chinese, SearchLanguage::Pinyin}};
);

class Kmoji : public fcitx::AddonInstance {
public:
    explicit Kmoji(fcitx::Instance *instance) : instance_(instance) {
        fcitx::registerDomain("kmoji", KMOJI_LOCALEDIR);
        reloadConfig();
        handlers_.push_back(instance_->watchEvent(fcitx::EventType::InputContextKeyEvent,
            fcitx::EventWatcherPhase::PreInputMethod, [this](fcitx::Event &ev) {
                auto &key = static_cast<fcitx::KeyEvent &>(ev);
                auto *ic = key.inputContext();
                if (active_ && ic == context_.get()) {
                    key.filterAndAccept();
                    if (!key.isRelease() && key.key().sym() == FcitxKey_Escape) { dismiss(); return; }
                    send("KEY\t" + std::to_string(token_) + "\t" + std::to_string(key.key().sym()) + "\t"
                         + std::to_string(key.key().states().toInteger()) + "\t" + (key.isRelease() ? "1" : "0")
                         + "\t" + hex(fcitx::Key::keySymToUTF8(key.key().sym())));
                    return;
                }
                if (key.isRelease() || !key.key().checkKeyList(*config_.triggerKey)) return;
                if (ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive) return;
                if (!ic->inputPanel().clientPreedit().empty() || !ic->inputPanel().preedit().empty()) return;
                if (!ensureServer()) return;
                key.filterAndAccept();
                active_ = true;
                context_ = ic->watch();
                ++token_;
                FCITX_LOGC(kmojiLog, Debug) << "Open session " << token_ << " focus=" << ic->hasFocus();
                startupKeys_.clear();
                if (client_ >= 0) show();
                else {
                    const char *overridePath = std::getenv("KMOJI_UI");
                    std::vector<std::string> args{overridePath ? overridePath : KMOJI_UI_PATH, "--socket", path_};
                    if (config_.language.value() == UiLanguage::SimplifiedChinese) args.push_back("--lang=zh_CN");
                    else if (config_.language.value() == UiLanguage::English) args.push_back("--lang=en");
                    if (!themeName().empty()) args.push_back("--theme=" + themeName());
                    args.push_back("--accent=" + accentName());
                    std::string hoverLanguages;
                    for (const auto language : config_.hoverLanguages.value())
                        hoverLanguages += (hoverLanguages.empty() ? "" : ",") +
                            std::string(language == HoverLanguage::English ? "en" : "zh");
                    args.push_back("--hover-langs=" + hoverLanguages);
                    std::string searchLanguages = "en";
                    for (const auto language : config_.searchLanguages.value())
                        searchLanguages += language == SearchLanguage::Chinese ? ",zh" : ",pinyin";
                    args.push_back("--search-langs=" + searchLanguages);
                    fcitx::startProcess(args);
                    startupTimer_ = instance_->eventLoop().addTimeEvent(CLOCK_MONOTONIC,
                        fcitx::now(CLOCK_MONOTONIC)+4000000, 0, [this](auto *, auto) { dismiss(); return false; });
                    startupTimer_->setOneShot();
                }
            }));
        for (auto type : {fcitx::EventType::InputContextFocusOut, fcitx::EventType::InputContextReset,
                          fcitx::EventType::InputContextDestroyed, fcitx::EventType::InputContextSwitchInputMethod}) {
            handlers_.push_back(instance_->watchEvent(type, fcitx::EventWatcherPhase::PreInputMethod,
                [this](fcitx::Event &ev) {
                    if (static_cast<fcitx::InputContextEvent &>(ev).inputContext() == context_.get()) {
                        FCITX_LOGC(kmojiLog, Debug) << "Cancel on event " << static_cast<uint32_t>(ev.type());
                        dismiss();
                    }
                }));
        }
    }
    ~Kmoji() override {
        send("QUIT");
        clientSource_.reset(); acceptSource_.reset();
        if (client_ >= 0) close(client_);
        if (server_ >= 0) close(server_);
        if (!path_.empty()) unlink(path_.c_str());
    }
private:
    void reloadConfig() override {
        fcitx::readAsIni(config_, "conf/kmoji.conf");
        dropClient();
    }
    const fcitx::Configuration *getConfig() const override { return &config_; }
    void setConfig(const fcitx::RawConfig &raw) override {
        config_.load(raw, true);
        fcitx::safeSaveAsIni(config_, "conf/kmoji.conf");
        dropClient();
    }
    std::string themeName() const {
        if (config_.theme.value() == UiTheme::Light) return "light";
        if (config_.theme.value() == UiTheme::Dark) return "dark";
        return {};
    }
    std::string accentName() const {
        std::string accent = config_.accentColor.value().toString();
        if (accent.size() == 9 && accent[0] == '#') accent.resize(7);
        return accent;
    }
    void dropClient() {
        if (client_ < 0) return;
        // Theme, accent, language and search settings are read when the UI
        // process starts. Drop the idle process so the next open picks them up.
        send("QUIT");
        if (clientSource_) clientSource_->setEnabled(false);
        close(client_);
        client_ = -1;
        active_ = false;
        context_.unwatch();
        buffer_.clear();
    }
    bool ensureServer() {
        if (server_ >= 0) return true;
        const char *runtime = std::getenv("XDG_RUNTIME_DIR");
        if (!runtime) return false;
        path_ = std::string(runtime)+"/kmoji-"+std::to_string(getpid())+".sock";
        if (path_.size() >= sizeof(sockaddr_un::sun_path)) return false;
        server_ = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (server_ < 0) return false;
        sockaddr_un address{}; address.sun_family = AF_UNIX;
        std::copy(path_.begin(), path_.end(), address.sun_path);
        if (bind(server_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) || chmod(path_.c_str(), 0600) || listen(server_, 1)) {
            close(server_); server_ = -1; unlink(path_.c_str()); return false;
        }
        acceptSource_ = instance_->eventLoop().addIOEvent(server_, fcitx::IOEventFlag::In,
            [this](auto *, int fd, auto) {
                int client = accept4(fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (client < 0) return true;
                ucred credentials{}; socklen_t size = sizeof(credentials);
                if (getsockopt(client, SOL_SOCKET, SO_PEERCRED, &credentials, &size) || credentials.uid != getuid() || client_ >= 0) {
                    close(client); return true;
                }
                client_ = client;
                buffer_.clear();
                clientSource_ = instance_->eventLoop().addIOEvent(client_, fcitx::IOEventFlags{fcitx::IOEventFlag::In, fcitx::IOEventFlag::Hup},
                    [this](auto *source, int input, auto) {
                        char chunk[4096];
                        for (;;) {
                            ssize_t n = recv(input, chunk, sizeof(chunk), MSG_DONTWAIT);
                            if (n > 0) buffer_.append(chunk, n);
                            else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                                source->setEnabled(false); close(client_); client_ = -1; active_ = false;
                                context_.unwatch(); buffer_.clear(); return true;
                            } else if (errno == EINTR) continue;
                            else break;
                        }
                        if (buffer_.size() > 32768) { shutdown(input, SHUT_RDWR); return true; }
                        size_t newline;
                        while ((newline = buffer_.find('\n')) != std::string::npos) {
                            auto line = buffer_.substr(0, newline); buffer_.erase(0, newline+1); receive(line);
                        }
                        return true;
                    });
                return true;
            });
        return true;
    }
    void send(const std::string &line) {
        if (client_ < 0) {
            if (active_ && line.starts_with("KEY\t") && startupKeys_.size() < 128) startupKeys_.push_back(line);
            return;
        }
        const auto data = line+"\n";
        if (::send(client_, data.data(), data.size(), MSG_NOSIGNAL | MSG_DONTWAIT) != static_cast<ssize_t>(data.size())) {
            // Never block the input-method event loop on a stalled frontend.
            shutdown(client_, SHUT_RDWR); active_ = false; context_.unwatch();
        }
    }
    void show() {
        auto *ic = context_.get();
        FCITX_LOGC(kmojiLog, Debug) << "Show session " << token_ << " active=" << active_ << " context=" << bool(ic) << " focus=" << (ic && ic->hasFocus());
        if (!active_ || !ic || !ic->hasFocus()) return;
        const auto r = ic->cursorRect();
        send("SHOW\t"+std::to_string(token_)+"\t"+std::to_string(r.left())+"\t"+std::to_string(r.top()));
        for (const auto &key : startupKeys_) send(key);
        startupKeys_.clear();
        if (startupTimer_) startupTimer_->setEnabled(false);
    }
    void dismiss() {
        if (active_) send("HIDE\t"+std::to_string(token_));
        active_ = false; context_.unwatch(); startupKeys_.clear();
        if (startupTimer_) startupTimer_->setEnabled(false);
    }
    void receive(const std::string &line) {
        const auto parts = split(line);
        FCITX_LOGC(kmojiLog, Debug) << "UI command " << parts[0];
        if (parts[0] == "READY") { show(); return; }
        if (parts.size() < 2 || parts[1] != std::to_string(token_) || !active_) return;
        auto *ic = context_.get();
        if (!ic || !ic->hasFocus() || (ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive)) { dismiss(); return; }
        if (parts[0] == "CANCEL") { dismiss(); return; }
        if (parts[0] == "COMMIT" && parts.size() == 4) {
            auto text = unhex(parts[2]);
            if (text.empty() || !fcitx::utf8::validate(text)) return;
            ic->commitString(text);
            if (parts[3] != "keep") dismiss();
        }
    }
    fcitx::Instance *instance_;
    KmojiConfig config_;
    std::vector<std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>>> handlers_;
    fcitx::TrackableObjectReference<fcitx::InputContext> context_;
    std::unique_ptr<fcitx::EventSourceIO> acceptSource_, clientSource_;
    std::unique_ptr<fcitx::EventSourceTime> startupTimer_;
    int server_ = -1, client_ = -1;
    uint64_t token_ = 0;
    bool active_ = false;
    std::string path_, buffer_;
    std::vector<std::string> startupKeys_;
};
class KmojiFactory : public fcitx::AddonFactory {
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override { return new Kmoji(manager->instance()); }
};
FCITX_ADDON_FACTORY(KmojiFactory);
