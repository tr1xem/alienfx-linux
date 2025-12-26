#pragma once
#include <libusb-1.0/libusb.h>
#include <string>
#include <vector>

using namespace std;

namespace AlienFX_SDK {
class Functions;
}

namespace AlienFX_SDK {

// Statuses for v1-v3
#define ALIENFX_V2_READY 0x10
#define ALIENFX_V2_BUSY 0x11
#define ALIENFX_V2_UNKNOWN 0x12
// Statuses for apiv4
#define ALIENFX_V4_READY 33
#define ALIENFX_V4_BUSY 34
#define ALIENFX_V4_WAITCOLOR 35
#define ALIENFX_V4_WAITUPDATE 36
#define ALIENFX_V4_WASON 38
// Statuses for apiv5
#define ALIENFX_V5_STARTCOMMAND 0x8c
#define ALIENFX_V5_WAITUPDATE 0x80
#define ALIENFX_V5_INCOMMAND 0xcc

// Mapping flags:
#define ALIENFX_FLAG_POWER 1     // This is power button
#define ALIENFX_FLAG_INDICATOR 2 // This is indicator light (keep at lights off)

// Maximal buffer size across all device types
#define MAX_BUFFERSIZE 193

union Afx_colorcode // Atomic color structure
{
    struct {
        unsigned char b, g, r;
        unsigned char br; // Brightness
    };
    unsigned long ci;
};

struct Afx_icommand {
    int i;
    vector<unsigned char> vval;
};
struct Afx_light { // Light information block
    unsigned char lightid;
    union {
        struct {
            unsigned short flags;
            unsigned short scancode;
        };
        unsigned long data;
    };
    std::string name;
};

union Afx_groupLight {
    struct {
        unsigned short did, lid;
    };
    unsigned long lgh;
};

struct Afx_group { // Light group information block
    unsigned long gid;
    std::string name;
    std::vector<Afx_groupLight> lights;
};
enum Afx_Version {
    API_ACPI = 0, // 128
    //	API_V9 = 9, //193
    API_V8 = 8, // 65
    API_V7 = 7, // 65
    API_V6 = 6, // 65
    API_V5 = 5, // 64
    API_V4 = 4, // 34
    API_V3 = 3, // 12
    API_V2 = 2, // 9
    API_UNKNOWN = -1
};

struct Afx_device { // Single device data
    union {
        struct {
            unsigned short pid, vid; // IDs
        };
        unsigned long devID;
    };
    Functions *dev = NULL;                 // device control object pointer
    std::string name;                      // device name
    int version = API_UNKNOWN;             // API version used for this device
    Afx_colorcode white = {255, 255, 255}; // white balance
    unsigned char brightness = 255;        // global device brightness
    std::vector<Afx_light> lights;         // vector of lights defined
    bool arrived = false,
         present = false; // for newly arrived and present devices
};

struct Afx_grid {
    unsigned char id;
    unsigned char x, y;
    std::string name;
    Afx_groupLight *grid;
};

struct Afx_action {        // atomic light action phase
    unsigned char type;    // one of Action values - action type
    unsigned char time;    // How long this phase stay
    unsigned char tempo;   // How fast it should transform
    unsigned char r, g, b; // phase color
};

struct Afx_lightblock { // light action block
    unsigned char index;
    std::vector<Afx_action> act;
};

enum Action {
    AlienFX_A_Color = 0,
    AlienFX_A_Pulse = 1,
    AlienFX_A_Morph = 2,
    AlienFX_A_Breathing = 3,
    AlienFX_A_Spectrum = 4,
    AlienFX_A_Rainbow = 5,
    AlienFX_A_Power = 6
};

class Functions {
  private:
    libusb_device_handle *devHandle = NULL; // USB device handle, NULL if not
    void *ACPIdevice = NULL;                // ACPI device object pointer

    bool inSet = false;

    int length = -1;         // HID report length
    unsigned char chain = 1; // seq. number for APIv1-v3

    // support function for mask-based devices (v1-v3, v6)
    vector<Afx_icommand> *SetMaskAndColor(vector<Afx_icommand> *mods,
                                          Afx_lightblock *act,
                                          bool needInverse = false,
                                          unsigned long index = 0);

    // Support function to send data to USB device
    bool PrepareAndSend(const unsigned char *command,
                        vector<Afx_icommand> mods);
    bool PrepareAndSend(const unsigned char *command,
                        vector<Afx_icommand> *mods = NULL);

    // Add new light effect block for v8
    inline void AddV8DataBlock(unsigned char bPos, vector<Afx_icommand> *mods,
                               Afx_lightblock *act);

    // Add new color block for v5
    inline void AddV5DataBlock(unsigned char bPos, vector<Afx_icommand> *mods,
                               unsigned char index, Afx_action *act);

    // Support function to send whole power block for v1-v3
    void SavePowerBlock(unsigned char blID, Afx_lightblock *act, bool needSave,
                        bool needSecondary = false, bool needInverse = false);

    // Support function for APIv4 action set
    bool SetV4Action(Afx_lightblock *act);

    // return current device state
    unsigned char GetDeviceStatus();

    // Next command delay for APIv1-v3
    unsigned char WaitForReady();

    // After-reset delay for APIv1-v3
    unsigned char WaitForBusy();

  public:
    union {
        struct {
            unsigned short pid, vid; // Device IDs
        };
        unsigned long devID;
    };
    int version = API_UNKNOWN; // interface version, will stay at API_UNKNOWN if
                               // not initialized
    unsigned char bright = 64; // Last brightness set for device
    string description;        // device description

    ~Functions() = default;

    // Initialize device
    // If vid is 0 or absent, first device found into the system will be used,
    // otherwise device with this VID only. If pid is 0 or absent, first device
    // with any pid and given vid will be used, otherwise vid/pid pair. Returns
    // true if device found and initialized.
    bool AlienFXInitialize(unsigned short vidd = 0, unsigned short pidd = 0);

    // Check device and initialize data
    // vid/pid the same as above
    // Returns true if device found and initialized.
    bool AlienFXProbeDevice(libusb_device *device, unsigned short vid = 0,
                            unsigned short pid = 0);

    // Prepare to set lights
    bool Reset();

    // false - not ready, true - ready, 0xff - stalled
    unsigned char IsDeviceReady();

    // Basic color set for current device.
    // index - light ID
    // c - color action structure
    // It's a synonym of SetAction, but with one color action
    bool SetColor(unsigned char index, Afx_action c);

    // Set multiply lights to the same color. This only works for some API
    // devices, and emulated for other ones. lights - pointer to vector of light
    // IDs need to be set. c - color to set
    bool SetMultiColor(vector<unsigned char> *lights, Afx_action c);

    // Set multiply lights to different color.
    // act - pointer to vector of light control blocks (each define one light)
    // store - need to save settings into device memory (v1-v4)
    bool SetMultiAction(vector<Afx_lightblock> *act, bool store = false);

    // Set color to action
    // index - light ID
    // act - pointer to light actions vector
    bool SetAction(Afx_lightblock *act);

    // Set action for Power button and store other light colors as default
    // act - pointer to vector of light control blocks
    bool SetPowerAction(vector<Afx_lightblock> *act, bool save = false);

    // Hardware brightness for device, if supported
    // brightness - desired brightness (0 - off, 255 - full)
    // gbr - global device brightness from mappings
    // mappings - mappings list for v4 brightness set (it require light IDs
    // list) power - if true, power and indicator lights will be set too
    bool SetBrightness(unsigned char brightness, unsigned char gbr,
                       vector<Afx_light> *mappings, bool power);

    // Global (whole device) effect control for APIv5, v8
    // effType - effect type
    // mode - effect mode (off, steady, keypress, etc)
    // nc - number of colors (3 - spectrum)
    // tempo - effect tempo
    // act1 - first effect color
    // act2 - second effect color (not for all effects)
    bool SetGlobalEffects(unsigned char effType, unsigned char mode,
                          unsigned char nc, unsigned char tempo,
                          Afx_colorcode act1, Afx_colorcode act2);

    // Apply changes and update colors
    bool UpdateColors();

    // check global effects availability
    bool IsHaveGlobal();
};

class Mappings {
  private:
    std::vector<Afx_group> groups; // Defined light groups
    std::vector<Afx_grid> grids;   // Grid zones info

  public:
    vector<Afx_device> fxdevs; // main devices/mappings array
    unsigned activeLights = 0, // total number of active lights into the system
        activeDevices = 0;     // total number of active devices
    bool deviceListChanged = false; // Is list changed after last device scan?

    ~Mappings() = default;

    // Update device info after it found into the system
    void AlienFxUpdateDevice(Functions *dev);

    // Enumerate all alienware devices into the system
    // acc - link to AlienFan_SDK::Control object for ACPI lights
    // returns true if light device list was changed
    bool AlienFXEnumDevices(void *acc = NULL);

    // load light names from registry
    void LoadMappings();

    // save light names into registry
    void SaveMappings();

    // Set device brightness
    // dev - point to AFX device info
    // br - brightness level
    // power - set power/indicator lights too
    bool SetDeviceBrightness(Afx_device *dev, unsigned char br, bool power);

    // get saved light structure by device ID and light ID
    Afx_light *GetMappingByID(unsigned short pid, unsigned short vid);

    // get defined groups vector
    vector<Afx_group> *GetGroups();

    // get defined grids vector
    vector<Afx_grid> *GetGrids() { return &grids; };

    // get grid object by it's ID
    Afx_grid *GetGridByID(unsigned char id);

    // get device structure by PID/VID.
    // VID can be zero for any VID
    Afx_device *GetDeviceById(unsigned short pid, unsigned short vid = 0);

    // get device by VID/PID unsigned long.
    Afx_device *GetDeviceById(unsigned long devID);

    // get or add device structure by PID/VID
    // VID can be zero for any VID
    Afx_device *AddDeviceById(unsigned long devID);

    // find light mapping into device structure by light ID
    Afx_light *GetMappingByDev(Afx_device *dev, unsigned short LightID);

    // find light group by it's ID
    Afx_group *GetGroupById(unsigned long gid);

    // remove light mapping from device by id
    void RemoveMapping(Afx_device *dev, unsigned short lightID);

    // get light flags (Power, indicator, etc) from light structure
    int GetFlags(Afx_device *dev, unsigned short lightid);

    // get light flags (Power, indicator) by PID
    int GetFlags(unsigned short pid, unsigned short lightid);
};

} // namespace AlienFX_SDK
