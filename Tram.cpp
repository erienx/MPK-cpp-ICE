#include <Ice/Ice.h>
#include <SIP.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <map>

using namespace std;
using namespace SIP;

class TramImpl : public Tram {
    string stockNumber;
    TramStopPrx currentStop;
    LinePrx line;
    vector<PassengerPrx> passengers;
    int currentStopIndex = -1;

public:
    TramImpl(const string &sn) : stockNumber(sn) {}

    virtual TramStopPrx getLocation(const Ice::Current & = Ice::Current()) override {
        return currentStop;
    }

    virtual LinePrx getLine(const Ice::Current & = Ice::Current()) override {
        return line;
    }

    virtual void setLine(const LinePrx &l, const Ice::Current & = Ice::Current()) override {
        line = l;
        currentStopIndex = -1;
    }

    virtual StopList getNextStops(int howMany, const Ice::Current & = Ice::Current()) override {
        StopList result;

        if (!line || currentStopIndex < 0)
            return result;

        StopList allStops = line->getStops();
        if (allStops.empty() || currentStopIndex >= static_cast<int>(allStops.size()))
            return result;

        auto baseTime = std::chrono::system_clock::now();

        for (int i = currentStopIndex + 1; i < allStops.size() && result.size() < howMany; ++i) {
            int minutesToAdd = (i - currentStopIndex +1) * 5;
            auto futureTime = baseTime + std::chrono::minutes(minutesToAdd);
            std::time_t futureTimeT = std::chrono::system_clock::to_time_t(futureTime);
            std::tm *timeinfo = std::localtime(&futureTimeT);

            Time estimatedTime;
            estimatedTime.hour = timeinfo->tm_hour;
            estimatedTime.minute = timeinfo->tm_min;

            StopInfo stopWithTime = allStops[i];
            stopWithTime.time = estimatedTime;

            result.push_back(stopWithTime);
        }

        return result;
    }


    virtual void RegisterPassenger(const PassengerPrx &p, const Ice::Current & = Ice::Current()) override {
        passengers.push_back(p);
        cout << "passenger registered on tram " << endl;
    }

    virtual void UnregisterPassenger(const PassengerPrx &p, const Ice::Current & = Ice::Current()) override {
        auto it = remove(passengers.begin(), passengers.end(), p);
        if (it != passengers.end()) {
            passengers.erase(it, passengers.end());
            cout << "passenger unregistered from tram " << endl;
        }
    }

    virtual string getStockNumber(const Ice::Current & = Ice::Current()) override {
        return stockNumber;
    }

    TramPrx selfProxy;

    void setSelfProxy(const TramPrx &proxy) {
        selfProxy = proxy;
    }

    bool moveToNextStop() {
        if (!line || !selfProxy) {
            cerr << "cant move no line or proxy" << endl;
            return false;
        }

        StopList stops = line->getStops();
        if (stops.empty() || currentStopIndex + 1 >= static_cast<int>(stops.size())) {
            cout << "end reached" << endl;
            return false;
        }

        currentStopIndex++;
        currentStop = stops[currentStopIndex].stop;

        Time arrivalTime = getCurrentTime();
        cout << "Arrived at stop: " << currentStop->getName()
             << " at " << arrivalTime.hour << ":" << arrivalTime.minute << endl;

        try {
            currentStop->UpdateTramInfo(selfProxy, arrivalTime);
            updateTimeAtStops();
            notifyPassengers();
            return true;
        } catch (const exception &ex) {
            cerr << "update failed: " << endl;
            return false;
        }
    }
    Time getCurrentTime() {
        auto now = chrono::system_clock::now();
        time_t nowT = chrono::system_clock::to_time_t(now);
        tm *timeinfo = localtime(&nowT);

        Time t;
        t.hour = timeinfo->tm_hour;
        t.minute = timeinfo->tm_min;
        return t;
    }




    string getCurrentStopName() {
        if (!currentStop) return "not at a stop";
        return currentStop->getName();
    }

private:
    void updateTimeAtStops() {
        if (!line) return;

        StopList allStops = line->getStops();
        if (currentStopIndex < 0 || currentStopIndex >= static_cast<int>(allStops.size())) return;

        auto baseTime = std::chrono::system_clock::now();

        for (int i = currentStopIndex + 1; i < allStops.size(); ++i) {
            int minutesToAdd = (i - currentStopIndex) * 5;
            auto futureTime = baseTime + std::chrono::minutes(minutesToAdd);
            std::time_t futureTimeT = std::chrono::system_clock::to_time_t(futureTime);
            std::tm *timeinfo = std::localtime(&futureTimeT);

            Time estimatedTime;
            estimatedTime.hour = timeinfo->tm_hour;
            estimatedTime.minute = timeinfo->tm_min;

            try {
                allStops[i].stop->UpdateTramInfo(selfProxy, estimatedTime);
            } catch (const std::exception &ex) {
                std::cerr << "cant update stop " << allStops[i].stop << std::endl;
            }
        }
    }



    void notifyPassengers() {
        StopList upcomingStops = getNextStops(3);
        vector<PassengerPrx> passengersCopy;
        {
            passengersCopy = passengers;
        }

        for (auto &passenger: passengersCopy) {
            try {
                passenger->updateTramInfo(selfProxy, upcomingStops);
            } catch (const exception &ex) {
                cerr << "Failed to update passenger: " << ex.what() << endl;
            }
        }
    }
};


int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <stock_number>" << endl;
        return 1;
    }

    string stockNumber = argv[1];
    int tramNumber;

    try {
        tramNumber = stoi(stockNumber);
        if (tramNumber >= 1000 || tramNumber < 0) {
            cerr << "invalid tram number (must be between 0 and 999)" << endl;
            return 2;
        }
    } catch (const exception &ex) {
        cerr << "invalid tram number: " << ex.what() << endl;
        return 2;
    }

    int status = 0;
    Ice::CommunicatorPtr ic;
    bool running = true;

    try {
        ic = Ice::initialize(argc, argv);

        stringstream endpoint;
        int port = 9000 + tramNumber;
        endpoint << "default -p " << port;

        Ice::ObjectAdapterPtr adapter = ic->createObjectAdapterWithEndpoints("TramAdapter", endpoint.str());

        TramImpl *tramImpl = new TramImpl(stockNumber);
        Ice::ObjectPtr tramObj = tramImpl;

        string tramIdentity = "Tram" + stockNumber;
        adapter->add(tramObj, Ice::stringToIdentity(tramIdentity));
        adapter->activate();

        cout << "tram " << stockNumber << " running on port started on port " << port << endl;

        TramPrx tramProxy = TramPrx::uncheckedCast(adapter->createProxy(Ice::stringToIdentity(tramIdentity)));

        tramImpl->setSelfProxy(tramProxy);

        MPKPrx mpkProxy;
        try {
            mpkProxy = MPKPrx::uncheckedCast(ic->stringToProxy("MPK:default -p 10000"));
            cout << "connected to mpk" << endl;
        } catch (const exception &ex) {
            cerr << "cant connect to mpk " << endl;
            return 3;
        }

        LineList lines;
        try {
            lines = mpkProxy->getLines();
            if (lines.empty()) {
                cerr << "no lines available " << endl;
                return 4;
            }

            LinePrx lineProxy = lines[0];
            tramImpl->setLine(lineProxy);

            try {
                lineProxy->registerTram(tramProxy);
                cout << "registered on line: " << lineProxy->getName() << endl;
            } catch (const exception &ex) {
                cerr << "cant register on line: " << endl;
            }
        } catch (const exception &ex) {
            cerr << "cant get lines " << endl;
            return 5;
        }

        try {
            DepoList depos = mpkProxy->getDepos();
            if (!depos.empty()) {
                DepoPrx depoProxy = depos[0].stop;
                depoProxy->TramOnline(tramProxy);
                cout << "registered tram at: " << depos[0].name << endl;
            }
        } catch (const exception &ex) {
            cerr << "cant register at depo 0" << endl;
        }

        cout << "commands:" << endl;
        cout << "  move        - move to next stop" << endl;
        cout << "  line <name> - connect to a different line" << endl;
        cout << "  exit        - exit" << endl;
        string command;

        while (running) {
            cout << "\nenter command: ";
            getline(cin, command);

            istringstream iss(command);
            string cmd;
            iss >> cmd;

            if (cmd == "exit") {
                running = false;
                cout << "closing..." << endl;
            } else if (cmd == "move") {
                if (tramImpl->moveToNextStop()) {
                    cout << "moved to stop: " << tramImpl->getCurrentStopName() << endl;
                } else {
                    cout << "cant move, maybe reached end of line" << endl;
                }
            } else if (cmd == "line") {
                string lineName;
                iss >> lineName;

                LinePrx newLine;
                bool found = false;

                for (const auto &line: lines) {
                    if (line->getName() == lineName) {
                        newLine = line;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    cout << "line not found" << endl;
                    continue;
                }

                LinePrx currentLine = tramImpl->getLine();
                if (currentLine) {
                    try {
                        currentLine->unregisterTram(tramProxy);
                        cout << "unregistered from current line: " << endl;
                    } catch (const exception &ex) {
                        cerr << "unregister from line fail " << endl;
                    }
                }

                try {
                    tramImpl->setLine(newLine);
                    newLine->registerTram(tramProxy);
                    cout << "registered on line: " << newLine->getName() << endl;
                } catch (const exception &ex) {
                    cerr << "register to line fail " << endl;
                }
            } else {
                cout << "unknown command" << endl;
            }
        }

        LinePrx line = tramImpl->getLine();
        if (line) {
            try {
                line->unregisterTram(tramProxy);
                cout << "unregistered from line" << endl;
            } catch (...) {
            }
        }

        if (ic) {
            ic->destroy();
        }

    } catch (const exception &ex) {
        cerr << "Error: " << ex.what() << endl;
        status = 1;
    }

    return status;
}