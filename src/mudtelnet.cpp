#include "mudtelnet/mudtelnet.h"
#include <boost/algorithm/string.hpp>

namespace mudtelnet {
    
    namespace codes {

        const char8_t NUL = 0, BEL = 7, CR = 13, LF = 10, SGA = 3, TELOPT_EOR = 25, NAWS = 31;
        const char8_t LINEMODE = 34, EOR = 239, SE = 240, NOP = 241, GA = 249, SB = 250;
        const char8_t WILL = 251, WONT = 252, DO = 253, DONT = 254, IAC = 255, MNES = 39;
        const char8_t MXP = 91, MSSP = 70, MCCP2 = 86, MCCP3 = 87, GMCP = 201, MSDP = 69;
        const char8_t MTTS = 24;
    }

    std::size_t TelnetMessage::parse(const std::string& buf) {
        using namespace codes;
        // return early if nothing to do.
        auto available = buf.length();
        if(!available) return 0;

        // So we do have some data?
        auto begin = buf.begin(), end = buf.end();
        bool escaped = false;

        // first, we read ahead
        if((char8_t)*begin == IAC) {
            // If it begins with an IAC, then it's a Command, Negotiation, or Subnegotiation
            if(available < 2) {
                return 0; // not enough bytes available - do nothing;
            }
            // we have 2 or more bytes!
            auto b = begin;
            b++;
            auto sub = b;
            char8_t option = 0;

            switch((char8_t)*b) {
                case WILL:
                case WONT:
                case DO:
                case DONT:
                    // This is a negotiation.
                    if(available < 3) return 0; // negotiations require at least 3 bytes.
                    msg_type = TelnetMsgType::Negotiation;
                    codes[0] = *b;
                    codes[1] = *(++b);
                    return 3;
                case SB:
                    // This is a subnegotiation. We need at least 5 bytes for it to work.
                    if(available < 5) return 0;

                    option = *(++b);
                    b++;
                    sub = b;
                    // we must seek ahead until we have an unescaped IAC SE. If we don't have one, do nothing.

                    while(b != end) {
                        if(escaped) {
                            escaped = false;
                            b++;
                            continue;
                        }
                        if((char8_t)*b == IAC) {
                            b++;
                            if(b != end && (char8_t)*b == SE) {
                                // we have a winner!
                                msg_type = TelnetMsgType::Subnegotiation;
                                codes[0] = option;
                                b--;
                                std::copy(sub, b, std::back_inserter(data));
                                return 5 + data.size();
                            } else {
                                escaped = true;
                                b--;
                                continue;
                            }

                        } else {
                            b++;
                        }
                    }
                    // if we finished the while loop, we don't have enough data, so...
                    return 0;
                default:
                    // if it's any other kind of IAC, it's a Command.
                    msg_type = TelnetMsgType::Command;
                    codes[0] = (char8_t)*(++b);
                    return 2;
            };
        } else {
            // Data begins on something that isn't an IAC. Scan ahead until we reach one...
            // Send all data up to an IAC, or everything if there is no IAC, as data.
            msg_type = TelnetMsgType::AppData;
            auto check = std::find(begin, end, IAC);
            std::copy(begin, check, std::back_inserter(data));
            return data.size();
        }
    }
    
    std::string TelnetMessage::toString() const {
        std::string out;
        
        switch(msg_type) {
            case TelnetMsgType::AppData:
                return data;
            case TelnetMsgType::Command:
                out.push_back(codes::IAC);
                out.push_back(codes[0]);
                break;
            case TelnetMsgType::Negotiation:
                out.push_back(codes::IAC);
                out.push_back(codes[0]);
                out.push_back(codes[1]);
                break;
            case TelnetMsgType::Subnegotiation:
                out.push_back(codes::IAC);
                out.push_back(codes::SB);
                out.push_back(codes[0]);
                out += data;
                out.push_back(codes::IAC);
                out.push_back(codes::SE);
                break;
        }
        
        return out;
    }

    TelnetOption::TelnetOption(MudTelnet *prot, char8_t code) : conn(prot) {
        this->code = code;
    }

    char8_t TelnetOption::opCode() const {
        return code;
    }

    bool TelnetOption::startDo() const {
        using namespace codes;
        switch(code) {
            case NAWS:
            case MTTS:
                return true;
            default:
                return false;
        }
    }

    bool TelnetOption::supportLocal() const {
        using namespace codes;
        switch(code) {
            //case MCCP2:
            case MSSP:
            case SGA:
            case MSDP:
            case GMCP:
                //case MXP:
                return true;
            default:
                return false;
        }
    }

    bool TelnetOption::supportRemote() const {
        using namespace codes;
        switch(code) {
            case NAWS:
            case MTTS:
                return true;
            default:
                return false;
        }
    }

    bool TelnetOption::startWill() const {
        using namespace codes;
        switch(code) {
            //case MCCP2:
            case MSSP:
            case SGA:
            case MSDP:
            case GMCP:
                //case MXP:
                return true;
            default:
                return false;
        }
    }

    void TelnetOption::enableLocal() {

    }

    void TelnetOption::enableRemote() {
        using namespace codes;
        switch(code) {
            case MTTS:
                conn->capabilities->mtts = true;
                conn->sendSub(code, std::string({1}));
                break;
        }
    }

    void TelnetOption::disableLocal() {

    }

    void TelnetOption::disableRemote() {

    }

    void TelnetOption::receiveNegotiate(char8_t command) {
        using namespace codes;
        switch(command) {
            case WILL:
                if(supportRemote()) {
                    if(remote.negotiating) {
                        remote.negotiating = false;
                        if(!remote.enabled) {
                            remote.enabled = true;
                            enableRemote();
                            if(!remote.answered) {
                                remote.answered = true;
                            }
                        }
                    } else {
                        remote.enabled = true;
                        conn->sendNegotiate(DO, opCode());
                        enableRemote();
                        if(!remote.answered) {
                            remote.answered = true;
                        }
                    }
                } else {
                    conn->sendNegotiate(DONT, opCode());
                }
                break;
            case DO:
                if(supportLocal()) {
                    if(local.negotiating) {
                        local.negotiating = false;
                        if(!local.enabled) {
                            local.enabled = true;
                            enableLocal();
                            if(!local.answered) {
                                local.answered = true;
                            }
                        }
                    } else {
                        local.enabled = true;
                        conn->sendNegotiate(WILL, opCode());
                        enableLocal();
                        if(!local.answered) {
                            local.answered = true;
                        }
                    }
                } else {
                    conn->sendNegotiate(WONT, opCode());
                }
                break;
            case WONT:
                if(remote.enabled) disableRemote();
                if(remote.negotiating) {
                    remote.negotiating = false;
                    if(!remote.answered) {
                        remote.answered = true;
                    }
                }
                break;
            case DONT:
                if(local.enabled) disableLocal();
                if(local.negotiating) {
                    local.negotiating = false;
                    if(!local.answered) {
                        local.answered = true;
                    }
                }
                break;
            default:
                break;
        }
    }

    void TelnetOption::subNegotiate(const TelnetMessage &msg) {
        using namespace codes;
        switch(code) {
            case MTTS:
                subMTTS(msg);
                break;
        }
    }

    void TelnetOption::subMTTS(const TelnetMessage &msg) {
        if(msg.data.empty()) return; // we need data to be useful.
        if(msg.data[0] != 0) return; // this is invalid MTTS.
        if(msg.data.size() < 2) return; // we need at least some decent amount of data to be useful.

        std::string mtts = boost::algorithm::to_upper_copy(std::string(msg.data.begin(), msg.data.end()).substr(1));

        if(mtts == conn->mttsLast) // there is no more data to be gleaned from asking...
            return;

        switch(mtts_count) {
            case 0:
                subMTTS_0(mtts);
                break;
            case 1:
                subMTTS_1(mtts);
                break;
            case 2:
                subMTTS_2(mtts);
                break;
        }

        mtts_count++;
        // cache the results and request more info.
        conn->mttsLast = mtts;
        if(mtts_count >= 2) return; // there is no more info to request.
        conn->sendSub(code, std::string({1}));

    }

    void TelnetOption::subMTTS_0(const std::string& mtts) {
        std::vector<std::string> namecheck;
        auto to_check = boost::algorithm::to_upper_copy(mtts);
        boost::algorithm::split(namecheck, to_check, boost::algorithm::is_space());
        switch(namecheck.size()) {
            case 2:
                conn->capabilities->clientVersion = namecheck[1];
            case 1:
                conn->capabilities->clientName = namecheck[0];
                break;
        }

        auto &details = conn->capabilities;
        auto &name = details->clientName;
        auto &version = details->clientVersion;

        if((name == "ATLANTIS") || (name == "CMUD") || (name == "KILDCLIENT") || (name == "MUDLET") ||
           (name == "PUTTY") || (name == "BEIP") || (name == "POTATO") || (name == "TINYFUGUE") || (name == "MUSHCLIENT")) {
            details->colorType = std::max(details->colorType, XtermColor);
        }

        // all clients that support MTTS probably support ANSI...
        details->colorType = std::max(details->colorType, StandardColor);
    }

    void TelnetOption::subMTTS_1(const std::string& mtts) {

        std::vector<std::string> splitcheck;
        auto to_check = boost::algorithm::to_upper_copy(mtts);
        boost::algorithm::split(splitcheck, to_check, boost::algorithm::is_any_of("-"));

        auto &details = conn->capabilities;


        switch(splitcheck.size()) {
            case 2:
                if(splitcheck[1] == "256COLOR") {
                    details->colorType = std::max(details->colorType, XtermColor);
                } else if (splitcheck[1] == "TRUECOLOR") {
                    details->colorType = std::max(details->colorType, TrueColor);
                }
            case 1:
                if(splitcheck[0] == "ANSI") {
                    details->colorType = std::max(details->colorType, StandardColor);
                } else if (splitcheck[0] == "VT100") {
                    details->colorType = std::max(details->colorType, StandardColor);
                    details->vt100 = true;
                } else if(splitcheck[0] == "XTERM") {
                    details->colorType = std::max(details->colorType, XtermColor);
                    details->vt100 = true;
                }
                break;
        }
    }

    void TelnetOption::subMTTS_2(const std::string mtts) {
        std::vector<std::string> splitcheck;
        auto to_check = boost::algorithm::to_upper_copy(mtts);
        boost::algorithm::split(splitcheck, to_check, boost::algorithm::is_space());

        if(splitcheck.size() < 2) return;

        if(splitcheck[0] != "MTTS") return;

        int v = atoi(splitcheck[1].c_str());

        auto &details = conn->capabilities;

        // ANSI
        if(v & 1) {
            details->colorType = std::max(details->colorType, StandardColor);
        }

        // VT100
        if(v & 2) {
            details->vt100 = true;
        }

        // UTF8
        if(v & 4) {
            details->utf8 = true;
        }

        // XTERM256 colors
        if(v & 8) {
            details->colorType = std::max(details->colorType, XtermColor);
        }

        // MOUSE TRACKING - who even uses this?
        if(v & 16) {
            details->mouse_tracking = true;
        }

        // On-screen color palette - again, is this even used?
        if(v & 32) {
            details->osc_color_palette = true;
        }

        // client uses a screen reader - this is actually somewhat useful for blind people...
        // if the game is designed for it...
        if(v & 64) {
            details->screen_reader = true;
        }

        // PROXY - I don't think this actually works?
        if(v & 128) {
            details->proxy = true;
        }

        // TRUECOLOR - support for this is probably rare...
        if(v & 256) {
            details->colorType = std::max(details->colorType, TrueColor);
        }

        // MNES - Mud New Environment Standard support.
        if(v & 512) {
            details->mnes = true;
        }

        // mud server link protocol ???
        if(v & 1024) {
            details->mslp = true;
        }

    }

    MudTelnet::MudTelnet(mudtelnet::TelnetCapabilities *cap) : capabilities(cap) {
        using namespace codes;

        for(const auto &code : {MSSP, SGA, MSDP, GMCP, NAWS, MTTS}) {
            auto result = handlers.emplace(code, TelnetOption(this, code));
            if(!result.second) continue;
            auto &h = result.first->second;
            if(h.startWill()) {
                h.local.negotiating = true;
                sendNegotiate(WILL, code);
            }
            if(h.startDo()) {
                h.remote.negotiating = true;
                sendNegotiate(DO, code);
            }

        }

    }

    void MudTelnet::sendMessage(const mudtelnet::TelnetMessage &data) {
        outDataBuffer += data.toString();
    }

    void MudTelnet::sendSub(const char8_t op, const std::string &data) {
        TelnetMessage msg {.msg_type=Subnegotiation, .data=data, .codes={op, 0}};
        sendMessage(msg);
    }

    void MudTelnet::sendGMCP(const std::string &txt) {
        sendSub(codes::GMCP, txt);
    }

    void MudTelnet::sendText(const std::string &txt) {
        TelnetMessage msg {.msg_type=AppData, .data=txt};
        sendMessage(msg);
    }

    const static char prompt_codes[2] = {static_cast<char>(codes::IAC), static_cast<char>(codes::GA)};
    const static std::string prompt(prompt_codes);

    void MudTelnet::sendPrompt(const std::string &txt) {
        if(boost::algorithm::ends_with(txt, prompt)) sendText(txt);
        else sendText(txt + prompt);
    }

    void MudTelnet::sendLine(const std::string &txt) {
        if(boost::algorithm::ends_with(txt, "\r\n")) sendText(txt);
        else sendText(txt + "\r\n");
    }

    void MudTelnet::sendMSSP(const std::vector<std::tuple<std::string, std::string>> &data) {
        std::vector<std::string> pairs;
        for(auto &p : data) {
            pairs.push_back(std::get<0>(p) + std::get<1>(p));
        }
        sendSub(codes::MSSP, boost::algorithm::join(pairs, "\r\n"));
    }

    void MudTelnet::sendNegotiate(char8_t command, const char8_t option) {
        TelnetMessage msg {.msg_type=Negotiation, .codes={command, option}};
        sendMessage(msg);
    }

    void MudTelnet::handleMessage(const mudtelnet::TelnetMessage &msg) {
        switch(msg.msg_type) {
            case AppData:
                handleAppData(msg);
                break;
            case Command:
                handleCommand(msg);
                break;
            case Negotiation:
                handleNegotiate(msg);
                break;
            case Subnegotiation:
                handleSubnegotiate(msg);
                break;
        }
    }

    void MudTelnet::handleAppData(const TelnetMessage &msg) {
        GameMessage g;
        for(const auto& c : msg.data) {
            switch(c) {
                case '\n':
                    g.data = appDataBuffer;
                    appDataBuffer.clear();
                    pendingGameMessages.push_back(g);
                    break;
                case '\r':
                    // we just ignore these.
                    break;
                default:
                    appDataBuffer.push_back(c);
                    break;
            }
        }
    }

    void MudTelnet::handleCommand(const TelnetMessage &msg) {

    }

    void MudTelnet::handleNegotiate(const TelnetMessage &msg) {
        using namespace codes;
        if(!handlers.count(msg.codes[1])) {
            switch(msg.codes[0]) {
                case WILL:
                    sendNegotiate(DONT, msg.codes[1]);
                    break;
                case DO:
                    sendNegotiate(WONT, msg.codes[1]);
                    break;
                default:
                    break;
            }
            return;
        }
        auto &hand = handlers.at(msg.codes[1]);
        hand.receiveNegotiate(msg.codes[0]);
    }

    void MudTelnet::handleSubnegotiate(const TelnetMessage &msg) {
        if(handlers.count(msg.codes[0])) {
            auto &hand = handlers.at(msg.codes[0]);
            hand.subNegotiate(msg);
        }
    }

}