//
// Created by volund on 10/21/22.
//
#pragma once
#include <string>
#include <vector>
#include <list>
#include <unordered_map>

namespace mudtelnet {
    namespace codes {
        extern const char8_t NUL;
        extern const char8_t BEL;
        extern const char8_t CR;
        extern const char8_t LF;
        extern const char8_t SGA;
        extern const char8_t TELOPT_EOR;
        extern const char8_t NAWS;
        extern const char8_t LINEMODE;
        extern const char8_t EOR;
        extern const char8_t SE;
        extern const char8_t NOP;
        extern const char8_t GA;
        extern const char8_t SB;
        extern const char8_t WILL;
        extern const char8_t WONT;
        extern const char8_t DO;
        extern const char8_t DONT;
        extern const char8_t IAC;

        extern const char8_t MNES;
        extern const char8_t MXP;
        extern const char8_t MSSP;
        extern const char8_t MCCP2;
        extern const char8_t MCCP3;

        extern const char8_t GMCP;
        extern const char8_t MSDP;
        extern const char8_t MTTS;
    }

    class TelnetOption;

    enum TelnetMsgType : char8_t {
        AppData = 0, // random telnet bytes
        Command = 1, // an IAC <something>
        Negotiation = 2, // an IAC WILL/WONT/DO/DONT
        Subnegotiation = 3 // an IAC SB <code> <data> IAC SE
    };

    struct TelnetMessage {
        TelnetMsgType msg_type;
        std::string data;
        char8_t codes[2] = {0, 0};
        std::size_t parse(const std::string& buf);
        std::string toString() const;
    };

    struct TelnetOptionPerspective {
        bool enabled = false, negotiating = false, answered = false;
    };

    enum GameMessageType {
        TextCommand = 0,
        JSON = 1
    };

    struct GameMessage {
        GameMessageType gameMessageType;
        std::string data;
    };

    class MudTelnet;
    
    class TelnetOption {
    public:
        TelnetOption(MudTelnet *prot, char8_t code);
        char8_t opCode() const;
        bool startWill() const, startDo() const, supportLocal() const, supportRemote() const;
        void enableLocal(), enableRemote(), disableLocal(), disableRemote();
        void receiveNegotiate(char8_t command);
        void subNegotiate(const TelnetMessage &msg);
        void rejectLocalHandshake(), acceptLocalHandshake(), rejectRemoteHandshake(), acceptRemoteHandshake();
        TelnetOptionPerspective local, remote;
        char8_t code;
    protected:
        MudTelnet *conn;
        int mtts_count = 0;
        void subMTTS(const TelnetMessage &msg);
        void subMTTS_0(const std::string& mtts);
        void subMTTS_1(const std::string& mtts);
        void subMTTS_2(const std::string mtts);
    };

    enum ColorType : uint8_t {
        NoColor = 0,
        StandardColor = 1,
        XtermColor = 2,
        TrueColor = 3
    };

    enum TextType : uint8_t {
        Text = 0,
        Line = 1,
        Prompt = 2
    };

    struct TelnetCapabilities {
        ColorType colorType = NoColor;
        std::string clientName = "UNKNOWN", clientVersion = "UNKNOWN";
        std::string hostIp = "UNKNOWN", hostName = "UNKNOWN";
        int width = 78, height = 24;
        bool utf8 = false, screen_reader = false, proxy = false, osc_color_palette = false;
        bool vt100 = false, mouse_tracking = false, naws = false, msdp = false, gmcp = false;
        bool mccp2 = false, mccp2_active = false, mccp3 = false, mccp3_active = false, telopt_eor = false;
        bool mtts = false, ttype = false, mnes = false, suppress_ga = false, mslp = false;
        bool force_endline = false, linemode = false, mssp = false, mxp = false, mxp_active = false;
    };

    class MudTelnet {
    public:
        explicit MudTelnet(TelnetCapabilities *cap);
        void sendMessage(const TelnetMessage& data);
        void sendGMCP(const std::string &txt);
        void sendPrompt(const std::string &txt);
        void sendLine(const std::string &txt);
        void sendText(const std::string &txt);
        void sendMSSP(const std::vector<std::tuple<std::string, std::string>> &data);
        void sendSub(char8_t op, const std::string& data);
        void sendNegotiate(char8_t command, char8_t option);
        void handleMessage(const TelnetMessage &msg);
        std::string appDataBuffer;
        std::list<GameMessage> pendingGameMessages;
        std::string outDataBuffer;
        TelnetCapabilities *capabilities;
        std::string mttsLast;
    protected:
        void handleAppData(const TelnetMessage &msg);
        void handleCommand(const TelnetMessage &msg);
        void handleNegotiate(const TelnetMessage &msg);
        void handleSubnegotiate(const TelnetMessage &msg);
        std::unordered_map<char8_t, TelnetOption> handlers;
    };

}