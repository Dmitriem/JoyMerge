#include "joymerge.h"
#include <android/log.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/input-event-codes.h>
#include <ctime>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "JoyMerge", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "JoyMerge", __VA_ARGS__)

static int ufd = -1;                 
static std::atomic<bool> running{false};
static pthread_t reader_thread;

static PadState g;

static void emit(int fd, uint16_t type, uint16_t code, int32_t val) {
    input_event ev{};
    ev.type=type; ev.code=code; ev.value=val;
    clock_gettime(CLOCK_MONOTONIC, &ev.time);
    write(fd, &ev, sizeof(ev));
}

static void syn(int fd) { emit(fd, EV_SYN, SYN_REPORT, 0); }

static bool setup_uinput() {
    ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (ufd < 0) { LOGE("open /dev/uinput failed"); return false; }

    ioctl(ufd, UI_SET_EVBIT, EV_KEY);
    ioctl(ufd, UI_SET_EVBIT, EV_ABS);
    ioctl(ufd, UI_SET_EVBIT, EV_SYN);

    int keys[] = { BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST,
                   BTN_TL, BTN_TR, BTN_START, BTN_SELECT, BTN_MODE,
                   BTN_THUMBL, BTN_THUMBR };
    for (int k: keys) ioctl(ufd, UI_SET_KEYBIT, k);

    ioctl(ufd, UI_SET_ABSBIT, ABS_HAT0X);
    ioctl(ufd, UI_SET_ABSBIT, ABS_HAT0Y);
    ioctl(ufd, UI_SET_ABSBIT, ABS_X);
    ioctl(ufd, UI_SET_ABSBIT, ABS_Y);
    ioctl(ufd, UI_SET_ABSBIT, ABS_RX);
    ioctl(ufd, UI_SET_ABSBIT, ABS_RY);
    ioctl(ufd, UI_SET_ABSBIT, ABS_Z);
    ioctl(ufd, UI_SET_ABSBIT, ABS_RZ);

    uinput_setup us{};
    us.id.bustype = BUS_USB;
    us.id.vendor  = 0x045E;
    us.id.product = 0x028E;
    snprintf(us.name, sizeof(us.name), "JoyMerge Virtual XInput");

    auto abs = [&](int code, int minv, int maxv, int fuzz=2, int flat=2000){
        uinput_abs_setup s{}; s.code=code; s.absinfo.minimum=minv; s.absinfo.maximum=maxv; s.absinfo.fuzz=fuzz; s.absinfo.flat=flat; ioctl(ufd, UI_ABS_SETUP, &s);
    };
    abs(ABS_X,  -32768, 32767);
    abs(ABS_Y,  -32768, 32767);
    abs(ABS_RX, -32768, 32767);
    abs(ABS_RY, -32768, 32767);
    abs(ABS_Z,      0, 1023, 0, 0);
    abs(ABS_RZ,     0, 1023, 0, 0);
    abs(ABS_HAT0X, -1, 1, 0, 0);
    abs(ABS_HAT0Y, -1, 1, 0, 0);

    if (ioctl(ufd, UI_DEV_SETUP, &us) < 0) { LOGE("UI_DEV_SETUP fail"); return false; }
    if (ioctl(ufd, UI_DEV_CREATE) < 0) { LOGE("UI_DEV_CREATE fail"); return false; }

    LOGI("uinput device created");
    return true;
}

static void destroy_uinput() {
    if (ufd >= 0) {
        ioctl(ufd, UI_DEV_DESTROY);
        close(ufd);
        ufd = -1;
        LOGI("uinput destroyed");
    }
}

static bool is_joycon_name(const char* name) {
    if (!name) return false;
    std::string s(name);
    return s.find("Joy-Con (L)") != std::string::npos || s.find("Joy-Con (R)") != std::string::npos;
}

static std::vector<int> open_joycon_fds() {
    std::vector<int> fds;
    DIR* d = opendir("/dev/input");
    if (!d) return fds;
    dirent* e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "event", 5) != 0) continue;
        std::string path = std::string("/dev/input/") + e->d_name;
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        char name[256] = {0};
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        if (is_joycon_name(name)) { fds.push_back(fd); LOGI("bind %s (%s)", path.c_str(), name); }
        else close(fd);
    }
    closedir(d);
    return fds;
}

static void send_full_state() {
    emit(ufd, EV_ABS, ABS_X,  g.left.x);
    emit(ufd, EV_ABS, ABS_Y,  g.left.y);
    emit(ufd, EV_ABS, ABS_RX, g.right.x);
    emit(ufd, EV_ABS, ABS_RY, g.right.y);
    emit(ufd, EV_ABS, ABS_Z,  g.trg.lt);
    emit(ufd, EV_ABS, ABS_RZ, g.trg.rt);
    emit(ufd, EV_ABS, ABS_HAT0X, g.hat_x);
    emit(ufd, EV_ABS, ABS_HAT0Y, g.hat_y);

    auto setbtn=[&](int bit, int code){ emit(ufd, EV_KEY, code, (g.buttons & (1u<<bit))?1:0); };
    setbtn(0, BTN_SOUTH); setbtn(1, BTN_EAST); setbtn(2, BTN_NORTH); setbtn(3, BTN_WEST);
    setbtn(4, BTN_TL); setbtn(5, BTN_TR); setbtn(6, BTN_START); setbtn(7, BTN_SELECT);
    setbtn(8, BTN_MODE); setbtn(9, BTN_THUMBL); setbtn(10, BTN_THUMBR);

    syn(ufd);
}

static void apply_event_from_joycon(const input_event& ev, bool isLeft) {
    if (ev.type == EV_ABS) {
        switch (ev.code) {
            case ABS_X: if (isLeft) g.left.x = ev.value; else g.right.x = ev.value; break;
            case ABS_Y: if (isLeft) g.left.y = ev.value; else g.right.y = ev.value; break;
            case ABS_RX: if (isLeft) g.right.x = ev.value; else g.left.x = ev.value; break;
            case ABS_RY: if (isLeft) g.right.y = ev.value; else g.left.y = ev.value; break;
            case ABS_Z:  if (isLeft) g.trg.lt = ev.value; else g.trg.rt = ev.value; break;
            case ABS_HAT0X: g.hat_x = ev.value; break;
            case ABS_HAT0Y: g.hat_y = ev.value; break;
        }
    } else if (ev.type == EV_KEY) {
        auto set = [&](int bit, int pressed){ if (pressed) g.buttons |= (1u<<bit); else g.buttons &= ~(1u<<bit); };
        switch (ev.code) {
            case BTN_EAST:  set(0, ev.value); break; // A
            case BTN_SOUTH: set(1, ev.value); break; // B
            case BTN_NORTH: set(2, ev.value); break; // X
            case BTN_WEST:  set(3, ev.value); break; // Y
            case BTN_TL:    set(4, ev.value); break; // L
            case BTN_TR:    set(5, ev.value); break; // R
            case BTN_START: set(6, ev.value); break; // +
            case BTN_SELECT:set(7, ev.value); break; // -
            case BTN_MODE:  set(8, ev.value); break; // HOME
            case BTN_THUMBL:set(9, ev.value); break; // L3
            case BTN_THUMBR:set(10, ev.value); break; // R3
        }
    }
}

static void* reader_main(void*) {
    auto fds = open_joycon_fds();
    if (fds.empty()) { LOGE("Joy-Con not found"); return nullptr; }

    std::vector<std::pair<int,bool>> devs;
    for (int fd: fds) {
        char name[256] = {0}; ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        bool isLeft = std::string(name).find("(L)") != std::string::npos;
        devs.emplace_back(fd, isLeft);
    }

    while (running.load()) {
        for (auto &p : devs) {
            input_event ev; ssize_t r = read(p.first, &ev, sizeof(ev));
            if (r == sizeof(ev)) { apply_event_from_joycon(ev, p.second); }
        }
        send_full_state();
        usleep(10000);
    }

    for (auto &p : devs) close(p.first);
    return nullptr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_joymerge_JoyMergeService_nativeStart(JNIEnv*, jobject, jboolean) {
    if (running.load()) return JNI_TRUE;
    if (!setup_uinput()) return JNI_FALSE;
    running.store(true);
    if (pthread_create(&reader_thread, nullptr, reader_main, nullptr) != 0) {
        running.store(false); destroy_uinput(); return JNI_FALSE;
    }
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_joymerge_JoyMergeService_nativeStop(JNIEnv*, jobject) {
    if (!running.load()) return;
    running.store(false);
    pthread_join(reader_thread, nullptr);
    destroy_uinput();
}
