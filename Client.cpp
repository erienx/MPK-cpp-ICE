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

// Global flags
bool running = true;
mutex mtx;
map<string, TramPrx> watchedTrams;
map<string, TramStopPrx> registeredStops;
map<string, Time> lastUpdatedTime;

class PassengerImpl : public Passenger {
public:
    virtual void updateTramInfo(const TramPrx& tram, const StopList& stops, const Ice::Current& = Ice::Current()) override {
        lock_guard<mutex> lock(mtx);

        string stockNumber = tram->getStockNumber();
        Time currentTime;

        // Get current time
        auto now = chrono::system_clock::now();
        time_t currentTimeT = chrono::system_clock::to_time_t(now);
        tm* timeinfo = localtime(&currentTimeT);
        currentTime.hour = timeinfo->tm_hour;
        currentTime.minute = timeinfo->tm_min;

        // Store update time
        lastUpdatedTime[stockNumber] = currentTime;

        cout << "\n[NOTIFICATION] Update for tram " << stockNumber << " at "
             << currentTime.hour << ":" << (currentTime.minute < 10 ? "0" : "") << currentTime.minute << endl;

        if (stops.empty()) {
            cout << "  No upcoming stops for this tram." << endl;
        } else {
            cout << "  Upcoming stops:" << endl;
            for (const auto& stop : stops) {
                cout << "  - " << stop.stop->getName() << " at "
                     << stop.time.hour << ":" << (stop.time.minute < 10 ? "0" : "") << stop.time.minute << endl;
            }
        }
        cout << "Enter command: ";
        cout.flush();
    }

    virtual void updateStopInfo(const TramStopPrx& stop, const TramList& trams, const Ice::Current& = Ice::Current()) override {
        lock_guard<mutex> lock(mtx);

        Time currentTime;
        // Get current time
        auto now = chrono::system_clock::now();
        time_t currentTimeT = chrono::system_clock::to_time_t(now);
        tm* timeinfo = localtime(&currentTimeT);
        currentTime.hour = timeinfo->tm_hour;
        currentTime.minute = timeinfo->tm_min;

        cout << "\n[NOTIFICATION] Update for stop " << stop->getName() << " at "
             << currentTime.hour << ":" << (currentTime.minute < 10 ? "0" : "") << currentTime.minute << endl;

        if (trams.empty()) {
            cout << "  No trams approaching this stop." << endl;
        } else {
            cout << "  Approaching trams:" << endl;
            for (const auto& tram : trams) {
                cout << "  - Tram " << tram.tram->getStockNumber() << " arriving at "
                     << tram.time.hour << ":" << (tram.time.minute < 10 ? "0" : "") << tram.time.minute << endl;
            }
        }
        cout << "Enter command: ";
        cout.flush();
    }
};

void printHelp() {
    cout << "\n==== MPK Passenger Client ====" << endl;
    cout << "Available commands:" << endl;
    cout << "  help                   - Show this help message" << endl;
    cout << "  stops                  - List all available stops" << endl;
    cout << "  register stop <name>   - Register at a stop to receive updates" << endl;
    cout << "  unregister stop <name> - Unregister from a stop" << endl;
    cout << "  lines                  - List all available lines" << endl;
    cout << "  line <name>            - Show details for a specific line" << endl;
    cout << "  trams <line>           - List trams on a specific line" << endl;
    cout << "  watch tram <number>    - Register with a tram to get updates" << endl;
    cout << "  unwatch tram <number>  - Unregister from a tram" << endl;
    cout << "  status                 - Show registration status" << endl;
    cout << "  exit                   - Exit the program" << endl;
    cout << "=============================" << endl;
}

int main(int argc, char* argv[]) {
    int status = 0;
    Ice::CommunicatorPtr ic;

    try {
        ic = Ice::initialize(argc, argv);

        // Create adapter for the client
        Ice::ObjectAdapterPtr adapter = ic->createObjectAdapterWithEndpoints(
                "ClientAdapter", "default -p 10002");

        // Create and register passenger object
        Ice::ObjectPtr passenger = new PassengerImpl();
        adapter->add(passenger, Ice::stringToIdentity("Passenger1"));
        adapter->activate();

        // Get passenger proxy for registration
        PassengerPrx passengerPrx = PassengerPrx::uncheckedCast(
                adapter->createProxy(Ice::stringToIdentity("Passenger1")));

        // Connect to MPK system
        MPKPrx mpk;
        try {
            Ice::ObjectPrx base = ic->stringToProxy("MPK:default -p 10000");
            mpk = MPKPrx::checkedCast(base);
            if (!mpk) {
                cerr << "Error: Invalid MPK proxy" << endl;
                return 1;
            }
            cout << "Connected to MPK information system" << endl;
        }
        catch (const exception& ex) {
            cerr << "Failed to connect to MPK system: " << ex.what() << endl;
            return 1;
        }

        // Command loop
        printHelp();
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
                    cout << "Shutting down client..." << endl;

                    // Unregister from all stops before exiting
                    for (const auto& stop : registeredStops) {
                        try {
                            stop.second->UnregisterPassenger(passengerPrx);
                            cout << "Unregistered from stop: " << stop.first << endl;
                        }
                        catch (...) {
                            // Ignore errors during shutdown
                        }
                    }

                    // Unregister from all trams before exiting
                    for (const auto& tram : watchedTrams) {
                        try {
                            tram.second->UnregisterPassenger(passengerPrx);
                            cout << "Unregistered from tram: " << tram.first << endl;
                        }
                        catch (...) {
                            // Ignore errors during shutdown
                        }
                    }
                }
                else if (cmd == "help") {
                    printHelp();
                }
                else if (cmd == "stops") {
                    cout << "\n==== Available Tram Stops ====" << endl;
                    try {
                        // Hard-coded for now, since MPK interface doesn't provide a method to list all stops
                        // In a real application, this would be dynamic
                        vector<string> stopNames = {"StopA", "StopB", "StopC"};

                        for (const auto& name : stopNames) {
                            try {
                                TramStopPrx stop = mpk->getTramStop(name);
                                cout << "- " << name;

                                // Check if registered
                                if (registeredStops.find(name) != registeredStops.end()) {
                                    cout << " (Registered)";
                                }

                                // Get next trams
                                TramList nextTrams = stop->getNextTrams(3);
                                if (!nextTrams.empty()) {
                                    cout << " - Next tram: " << nextTrams[0].tram->getStockNumber()
                                         << " at " << nextTrams[0].time.hour << ":"
                                         << (nextTrams[0].time.minute < 10 ? "0" : "") << nextTrams[0].time.minute;
                                }
                                cout << endl;
                            }
                            catch (...) {
                                cout << "- " << name << " (Unavailable)" << endl;
                            }
                        }
                    }
                    catch (const exception& ex) {
                        cout << "Error retrieving stops: " << ex.what() << endl;
                    }
                    cout << "=============================" << endl;
                }
                else if (cmd == "register") {
                    iss >> subcmd >> name;
                    if (subcmd == "stop" && !name.empty()) {
                        try {
                            // Check if already registered
                            if (registeredStops.find(name) != registeredStops.end()) {
                                cout << "Already registered at stop: " << name << endl;
                                continue;
                            }

                            TramStopPrx stop = mpk->getTramStop(name);
                            stop->RegisterPassenger(passengerPrx);
                            registeredStops[name] = stop;

                            cout << "Successfully registered at stop: " << name << endl;

                            // Show upcoming trams as initial info
                            TramList trams = stop->getNextTrams(5);
                            if (trams.empty()) {
                                cout << "No upcoming trams at this stop." << endl;
                            } else {
                                cout << "Upcoming trams:" << endl;
                                for (const auto& tram : trams) {
                                    cout << "- Tram " << tram.tram->getStockNumber() << " arriving at "
                                         << tram.time.hour << ":"
                                         << (tram.time.minute < 10 ? "0" : "") << tram.time.minute << endl;
                                }
                            }
                        }
                        catch (const exception& ex) {
                            cout << "Error registering at stop: " << ex.what() << endl;
                        }
                    } else {
                        cout << "Usage: register stop <name>" << endl;
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
                                cout << "Successfully unregistered from stop: " << name << endl;
                            } else {
                                cout << "Not registered at stop: " << name << endl;
                            }
                        }
                        catch (const exception& ex) {
                            cout << "Error unregistering from stop: " << ex.what() << endl;
                        }
                    } else {
                        cout << "Usage: unregister stop <name>" << endl;
                    }
                }
                else if (cmd == "lines") {
                    cout << "\n==== Available Lines ====" << endl;
                    try {
                        LineList lines = mpk->getLines();
                        if (lines.empty()) {
                            cout << "No lines available." << endl;
                        } else {
                            for (const auto& line : lines) {
                                cout << "- " << line->getName() << endl;
                            }
                        }
                    }
                    catch (const exception& ex) {
                        cout << "Error retrieving lines: " << ex.what() << endl;
                    }
                    cout << "========================" << endl;
                }
                else if (cmd == "line") {
                    iss >> name;
                    if (!name.empty()) {
                        try {
                            LineList lines = mpk->getLines();
                            LinePrx foundLine;
                            bool found = false;

                            for (const auto& line : lines) {
                                if (line->getName() == name) {
                                    foundLine = line;
                                    found = true;
                                    break;
                                }
                            }

                            if (!found) {
                                cout << "Line '" << name << "' not found." << endl;
                                continue;
                            }

                            cout << "\n==== Line " << foundLine->getName() << " Information ====" << endl;

                            // Get stops for this line
                            StopList stops = foundLine->getStops();
                            cout << "Stops (" << stops.size() << "):" << endl;
                            for (const auto& stop : stops) {
                                cout << "  - " << stop.stop->getName();
                                cout << " (Arrival time: " << stop.time.hour << ":"
                                     << (stop.time.minute < 10 ? "0" : "") << stop.time.minute << ")" << endl;
                            }

                            // Get trams for this line
                            TramList trams = foundLine->getTrams();
                            cout << "Trams (" << trams.size() << "):" << endl;
                            if (trams.empty()) {
                                cout << "  No trams currently on this line." << endl;
                            } else {
                                for (const auto& tram : trams) {
                                    cout << "  - Stock Number: " << tram.tram->getStockNumber() << endl;
                                }
                            }

                            cout << "=============================" << endl;
                        }
                        catch (const exception& ex) {
                            cout << "Error retrieving line information: " << ex.what() << endl;
                        }
                    } else {
                        cout << "Usage: line <name>" << endl;
                    }
                }
                else if (cmd == "trams") {
                    iss >> name;
                    if (!name.empty()) {
                        try {
                            LineList lines = mpk->getLines();
                            LinePrx foundLine;
                            bool found = false;

                            for (const auto& line : lines) {
                                if (line->getName() == name) {
                                    foundLine = line;
                                    found = true;
                                    break;
                                }
                            }

                            if (!found) {
                                cout << "Line '" << name << "' not found." << endl;
                                continue;
                            }

                            cout << "\n==== Trams on Line " << foundLine->getName() << " ====" << endl;

                            // Get trams for this line
                            TramList trams = foundLine->getTrams();
                            if (trams.empty()) {
                                cout << "No trams currently on this line." << endl;
                            } else {
                                for (const auto& tram : trams) {
                                    string stockNumber = tram.tram->getStockNumber();
                                    cout << "- Tram " << stockNumber;

                                    // Check if we're watching this tram
                                    if (watchedTrams.find(stockNumber) != watchedTrams.end()) {
                                        cout << " (Watching)";
                                    }

                                    // Get current location if available
                                    try {
                                        TramStopPrx location = tram.tram->getLocation();
                                        if (location) {
                                            cout << " - Current location: " << location->getName();
                                        }
                                    }
                                    catch (...) {
                                        // Ignore errors retrieving location
                                    }

                                    cout << endl;
                                }
                            }

                            cout << "=============================" << endl;
                        }
                        catch (const exception& ex) {
                            cout << "Error retrieving trams: " << ex.what() << endl;
                        }
                    } else {
                        cout << "Usage: trams <line_name>" << endl;
                    }
                }
                else if (cmd == "watch") {
                    iss >> subcmd >> name;
                    if (subcmd == "tram" && !name.empty()) {
                        try {
                            // Check if already watching
                            if (watchedTrams.find(name) != watchedTrams.end()) {
                                cout << "Already watching tram: " << name << endl;
                                continue;
                            }

                            // Find the tram
                            bool found = false;
                            LineList lines = mpk->getLines();

                            for (const auto& line : lines) {
                                TramList trams = line->getTrams();
                                for (const auto& tram : trams) {
                                    if (tram.tram->getStockNumber() == name) {
                                        tram.tram->RegisterPassenger(passengerPrx);
                                        watchedTrams[name] = tram.tram;
                                        found = true;

                                        cout << "Successfully registered with tram: " << name << endl;

                                        // Display immediate information
                                        StopList nextStops = tram.tram->getNextStops(3);
                                        if (nextStops.empty()) {
                                            cout << "No upcoming stops for this tram." << endl;
                                        } else {
                                            cout << "Upcoming stops:" << endl;
                                            for (const auto& stop : nextStops) {
                                                cout << "- " << stop.stop->getName() << " at "
                                                     << stop.time.hour << ":"
                                                     << (stop.time.minute < 10 ? "0" : "") << stop.time.minute << endl;
                                            }
                                        }

                                        break;
                                    }
                                }
                                if (found) break;
                            }

                            if (!found) {
                                cout << "Tram with stock number '" << name << "' not found." << endl;
                            }
                        }
                        catch (const exception& ex) {
                            cout << "Error registering with tram: " << ex.what() << endl;
                        }
                    } else {
                        cout << "Usage: watch tram <stock_number>" << endl;
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
                                cout << "Successfully unregistered from tram: " << name << endl;
                            } else {
                                cout << "Not watching tram: " << name << endl;
                            }
                        }
                        catch (const exception& ex) {
                            cout << "Error unregistering from tram: " << ex.what() << endl;
                        }
                    } else {
                        cout << "Usage: unwatch tram <stock_number>" << endl;
                    }
                }
                else if (cmd == "status") {
                    cout << "\n==== Client Status ====" << endl;

                    // Registered stops
                    cout << "Registered stops (" << registeredStops.size() << "):" << endl;
                    if (registeredStops.empty()) {
                        cout << "  Not registered at any stops." << endl;
                    } else {
                        for (const auto& stop : registeredStops) {
                            cout << "  - " << stop.first << endl;
                        }
                    }

                    // Watched trams
                    cout << "Watched trams (" << watchedTrams.size() << "):" << endl;
                    if (watchedTrams.empty()) {
                        cout << "  Not watching any trams." << endl;
                    } else {
                        for (const auto& tram : watchedTrams) {
                            cout << "  - Tram " << tram.first;

                            // Add last update time if available
                            auto it = lastUpdatedTime.find(tram.first);
                            if (it != lastUpdatedTime.end()) {
                                cout << " (Last update: " << it->second.hour << ":"
                                     << (it->second.minute < 10 ? "0" : "") << it->second.minute << ")";
                            }

                            cout << endl;
                        }
                    }

                    cout << "======================" << endl;
                }
                else {
                    cout << "Unknown command. Type 'help' for available commands." << endl;
                }
            }
            catch (const exception& ex) {
                cout << "Error executing command: " << ex.what() << endl;
            }
        }

        // Clean shutdown
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