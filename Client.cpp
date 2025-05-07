#include <Ice/Ice.h>
#include <SIP.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>

using namespace std;
using namespace SIP;

bool running = true;
mutex mtx;
map<string, TramPrx> watchedTrams;
map<string, TramStopPrx> registeredStops;
map<string, Time> lastUpdatedTime;

class PassengerImpl : public Passenger {
public:
    PassengerImpl(const string& clientId) : clientId(clientId) {}

    virtual void updateTramInfo(const TramPrx& tram, const StopList& stops, const Ice::Current& = Ice::Current()) override {
        lock_guard<mutex> lock(mtx);

        string stockNumber = tram->getStockNumber();
        Time currentTime;

        auto now = chrono::system_clock::now();
        time_t currentTimeT = chrono::system_clock::to_time_t(now);
        tm* timeinfo = localtime(&currentTimeT);
        currentTime.hour = timeinfo->tm_hour;
        currentTime.minute = timeinfo->tm_min;

        lastUpdatedTime[stockNumber] = currentTime;

        cout << "\n[NOTIFICATION] update for tram " << stockNumber << " at "<< currentTime.hour << ":" << currentTime.minute << endl;

        if (stops.empty()) {
            cout << "no upcoming stops" << endl;
        } else {
            cout << "upcoming stops" << endl;
            for (const auto& stop : stops) {
                cout << "  - " << stop.stop->getName() << " at "<< stop.time.hour << ":"  << stop.time.minute << endl;
            }
        }
        cout << "Enter command: ";
        cout.flush();
    }

    virtual void updateStopInfo(const TramStopPrx& stop, const TramList& trams, const Ice::Current& = Ice::Current()) override {
        lock_guard<mutex> lock(mtx);

        Time currentTime;
        auto now = chrono::system_clock::now();
        time_t currentTimeT = chrono::system_clock::to_time_t(now);
        tm* timeinfo = localtime(&currentTimeT);
        currentTime.hour = timeinfo->tm_hour;
        currentTime.minute = timeinfo->tm_min;

        cout << "\n[NOTIFICATION] update for stop " << stop->getName() << " at "<< currentTime.hour << ":" << currentTime.minute << endl;

        if (trams.empty()) {
            cout << "no trams inc" << endl;
        } else {
            cout << "inc trams: " << endl;
            for (const auto& tram : trams) {
                cout << "  - Tram " << tram.tram->getStockNumber() << " arriving at "<< tram.time.hour << ":"  << tram.time.minute << endl;
            }
        }
        cout << "Enter command: ";
        cout.flush();
    }
private:
    string clientId;

};



int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <client_id>" << endl;
        return 1;
    }

    string clientId = argv[1];
    int clientNum;

    try {
        clientNum = stoi(clientId);
        if (clientNum >= 1000 || clientNum < 0) {
            cerr << "Invalid client ID (must be between 0 and 999)" << endl;
            return 2;
        }
    } catch (const exception &ex) {
        cerr << "Invalid client ID: " << ex.what() << endl;
        return 2;
    }

    int status = 0;
    Ice::CommunicatorPtr ic;

    try {
        ic = Ice::initialize(argc, argv);

        // Create unique port for this client
        stringstream endpoint;
        int port = 10000 + clientNum;
        endpoint << "default -p " << port;

        Ice::ObjectAdapterPtr adapter = ic->createObjectAdapterWithEndpoints(
                "ClientAdapter", endpoint.str());

        Ice::ObjectPtr passenger = new PassengerImpl(clientId);
        string passengerIdentity = "Passenger" + clientId;
        adapter->add(passenger, Ice::stringToIdentity(passengerIdentity));
        adapter->activate();

        PassengerPrx passengerPrx = PassengerPrx::uncheckedCast(
                adapter->createProxy(Ice::stringToIdentity(passengerIdentity)));

        MPKPrx mpk;
        try {
            Ice::ObjectPrx base = ic->stringToProxy("MPK:default -p 10000");
            mpk = MPKPrx::checkedCast(base);
            if (!mpk) {
                cerr << "mpk proxy doesnt work" << endl;
                return 1;
            }
            cout << "Connected to mpk" << endl;
        }
        catch (const exception& ex) {
            cerr << "cant connect to mpk " << endl;
            return 1;
        }

        cout << "commands:" << endl;
        cout << "  register stop <name>   - register at stop for updates" << endl;
        cout << "  unregister stop <name> - unregister from a stop" << endl;
        cout << "  watch tram <number>    - register on a tram for updates" << endl;
        cout << "  unwatch tram <number>  - unregister from a tram" << endl;
        cout << "  exit                   - exit n" << endl;
        string command;

        while (running) {
            cout << "\nEnter command: ";
            getline(cin, command);

            istringstream iss(command);
            string cmd, subcmd, name;
            iss >> cmd;

            try {
                if (cmd == "exit") {
                    running = false;
                    cout << "closing.." << endl;

                    for (const auto& stop : registeredStops) {
                        try {
                            stop.second->UnregisterPassenger(passengerPrx);
                            cout << "Unregistered from stop: " << stop.first << endl;
                        }
                        catch (...) {
                        }
                    }

                    for (const auto& tram : watchedTrams) {
                        try {
                            tram.second->UnregisterPassenger(passengerPrx);
                            cout << "Unregistered from tram: " << tram.first << endl;
                        }
                        catch (...) {
                        }
                    }
                }

                else if (cmd == "register") {
                    iss >> subcmd >> name;
                    if (subcmd == "stop" && !name.empty()) {
                        try {
                            if (registeredStops.find(name) != registeredStops.end()) {
                                cout << "already registered on stop "<< endl;
                                continue;
                            }

                            TramStopPrx stop = mpk->getTramStop(name);
                            stop->RegisterPassenger(passengerPrx);
                            registeredStops[name] = stop;

                            cout << "registered at stop"<< endl;

                            TramList trams = stop->getNextTrams(5);
                            if (trams.empty()) {
                                cout << "no incoming trams" << endl;
                            } else {
                                cout << "incoming trams:" << endl;
                                for (const auto& tram : trams) {
                                    cout << "- Tram " << tram.tram->getStockNumber() << " arriving at "
                                         << tram.time.hour << ":" << tram.time.minute << endl;
                                }
                            }
                        }
                        catch (const exception& ex) {
                            cout << "register error : " <<endl;
                        }
                    } else {
                        cout << "provide stop name!" << endl;
                    }
                }
                else if (cmd == "unregister") {
                    iss >> subcmd >> name;
                    if (subcmd == "stop" && !name.empty()) {
                        try {
                            auto it = registeredStops.find(name);
                            if (it != registeredStops.end()) {
                                it->second->UnregisterPassenger(passengerPrx);
                                registeredStops.erase(it);
                                cout << "unregistered from stop " << endl;
                            } else {
                                cout << "not registered at stop " << endl;
                            }
                        }
                        catch (const exception& ex) {
                            cout << "err on unregister "<< endl;
                        }
                    } else {
                        cout << "provide stop name!" << endl;
                    }
                }

                else if (cmd == "watch") {
                    iss >> subcmd >> name;
                    if (subcmd == "tram" && !name.empty()) {
                        try {
                            if (watchedTrams.find(name) != watchedTrams.end()) {
                                cout << "already watching tram: " << name << endl;
                                continue;
                            }

                            bool found = false;
                            LineList lines = mpk->getLines();

                            for (const auto& line : lines) {
                                TramList trams = line->getTrams();
                                for (const auto& tram : trams) {
                                    if (tram.tram->getStockNumber() == name) {
                                        tram.tram->RegisterPassenger(passengerPrx);
                                        watchedTrams[name] = tram.tram;
                                        found = true;

                                        cout << "registered on tram " << name << endl;


                                        break;
                                    }
                                }
                                if (found) break;
                            }

                            if (!found) {
                                cout << "tram not found" << endl;
                            }
                        }
                        catch (const exception& ex) {
                            cout << "Error registering on tram" << endl;
                        }
                    } else {
                        cout << "provide tram number!" << endl;
                    }
                }
                else if (cmd == "unwatch") {
                    iss >> subcmd >> name;
                    if (subcmd == "tram" && !name.empty()) {
                        try {
                            auto it = watchedTrams.find(name);
                            if (it != watchedTrams.end()) {
                                it->second->UnregisterPassenger(passengerPrx);
                                watchedTrams.erase(it);
                                cout << "unregistered from tram "  << endl;
                            }
                        }
                        catch (const exception& ex) {
                            cout << "error unregistering  " << endl;
                        }
                    } else {
                        cout << "provide tram number!" << endl;
                    }
                }

                else {
                    cout << "unknown command." << endl;
                }
            }
            catch (const exception& ex) {
                cout << "error  " << endl;
            }
        }

        if (ic) {
            ic->destroy();
        }
    }
    catch (const exception& ex) {
        cerr << "Error: " << ex.what() << endl;
        status = 1;
    }

    return status;
}