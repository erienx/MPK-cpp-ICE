#include <Ice/Ice.h>
#include <SIP.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <map>
#include <mutex>
#include <condition_variable>

using namespace std;
using namespace SIP;

// Flag for stop movement simulation
bool autoMode = false;
bool running = true;
mutex mtx;
condition_variable cv;

class TramImpl : public Tram {
    string stockNumber;
    TramStopPrx currentStop;
    LinePrx line;
    vector<PassengerPrx> passengers;
    int currentStopIndex = -1; // No stop initially

public:
    TramImpl(const string& sn) : stockNumber(sn) {}

    virtual TramStopPrx getLocation(const Ice::Current& = Ice::Current()) override {
        return currentStop;
    }

    virtual LinePrx getLine(const Ice::Current& = Ice::Current()) override {
        return line;
    }

    virtual void setLine(const LinePrx& l, const Ice::Current& = Ice::Current()) override {
        line = l;
        currentStopIndex = -1; // Reset position when line changes
    }

    virtual StopList getNextStops(int howMany, const Ice::Current& = Ice::Current()) override {
        if (!line)
            return {};

        StopList allStops = line->getStops();
        if (allStops.empty() || currentStopIndex >= static_cast<int>(allStops.size()))
            return {};

        StopList out;
        int startIndex = (currentStopIndex < 0) ? 0 : currentStopIndex + 1;

        for (int i = startIndex; i < allStops.size() && (i - startIndex) < howMany; i++) {
            out.push_back(allStops[i]);
        }
        return out;
    }

    virtual void RegisterPassenger(const PassengerPrx& p, const Ice::Current& = Ice::Current()) override {
        passengers.push_back(p);
        cout << "Passenger registered on tram " << stockNumber << endl;
    }

    virtual void UnregisterPassenger(const PassengerPrx& p, const Ice::Current& = Ice::Current()) override {
        auto it = remove(passengers.begin(), passengers.end(), p);
        if (it != passengers.end()) {
            passengers.erase(it, passengers.end());
            cout << "Passenger unregistered from tram " << stockNumber << endl;
        }
    }

    virtual string getStockNumber(const Ice::Current& = Ice::Current()) override {
        return stockNumber;
    }

    // Non-Ice methods for tram movement logic
    // Store the tram proxy for use in updates
    TramPrx selfProxy;

    void setSelfProxy(const TramPrx& proxy) {
        selfProxy = proxy;
    }

    bool moveToNextStop() {
        if (!line)
            return false;

        StopList stops = line->getStops();
        if (stops.empty())
            return false;

        currentStopIndex++;
        if (currentStopIndex >= stops.size()) {
            cout << "End of line reached. Run completed." << endl;
            return false;
        }

        currentStop = stops[currentStopIndex].stop;

        // Update the list of upcoming stops for the current tram position
        StopList upcomingStops;
        int numStops = 3; // You can modify how many stops you want to show
        int endIndex = min(currentStopIndex + numStops, static_cast<int>(stops.size()));

        for (int i = currentStopIndex + 1; i < endIndex; ++i) {
            upcomingStops.push_back(stops[i]);
        }

        // Notify passengers about the new stop
        Time currentTime;
        auto now = chrono::system_clock::now();
        time_t currentTimeT = chrono::system_clock::to_time_t(now);
        tm* timeinfo = localtime(&currentTimeT);

        currentTime.hour = timeinfo->tm_hour;
        currentTime.minute = timeinfo->tm_min;

        // Notify each passenger with the updated list of upcoming stops
        for (auto& passenger : passengers) {
            try {
                // Send the updated upcoming stops and the tram info to each passenger
                passenger->updateTramInfo(selfProxy, upcomingStops);
                cout << "Notifying passenger about arrival at " << currentStop->getName() << endl;
            } catch (const exception& ex) {
                cerr << "Error notifying passenger: " << ex.what() << endl;
            }
        }

        // Update the stop with our arrival information using our proxy
        try {
            cout << "Arrived at stop: " << currentStop->getName() <<
                 " at " << currentTime.hour << ":" <<
                 (currentTime.minute < 10 ? "0" : "") << currentTime.minute << endl;

            // Update the stop with our arrival information using our proxy
            if (selfProxy) {
                currentStop->UpdateTramInfo(selfProxy, currentTime);
                return true;
            } else {
                cerr << "Error: Self proxy not set" << endl;
                return false;
            }
        } catch (const exception& ex) {
            cerr << "Error updating stop information: " << ex.what() << endl;
            return false;
        }
    }


    string getCurrentStopName() {
        if (!currentStop)
            return "Not at any stop";
        return currentStop->getName();
    }

    string getNextStopName() {
        StopList nextStops = getNextStops(1);
        if (nextStops.empty())
            return "End of line";
        return nextStops[0].stop->getName();
    }

    int getCurrentStopPosition() {
        return currentStopIndex;
    }

    bool isAtTerminus() {
        if (!line)
            return true;

        StopList stops = line->getStops();
        return currentStopIndex >= stops.size() - 1;
    }
};

// Function to run in a separate thread for automatic movement
void autoMoveThread(TramImpl* tram) {
    while (running) {
        {
            unique_lock<mutex> lock(mtx);
            if (!autoMode) {
                // Wait until autoMode is true
                cv.wait(lock, [] { return autoMode || !running; });
            }

            if (!running) break;
        }

        if (!tram->moveToNextStop()) {
            cout << "End of line reached, stopping auto mode" << endl;
            autoMode = false;
        }

        // Sleep for 5 seconds between stops
        this_thread::sleep_for(chrono::seconds(5));
    }
}

void printHelp() {
    cout << "\n==== Tram Control System ====" << endl;
    cout << "Available commands:" << endl;
    cout << "  status      - Show current tram status" << endl;
    cout << "  move        - Move to next stop" << endl;
    cout << "  auto        - Toggle automatic movement mode" << endl;
    cout << "  line <name> - Connect to a different line" << endl;
    cout << "  help        - Show this help message" << endl;
    cout << "  exit        - Exit the program" << endl;
    cout << "=============================" << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <stock_number>" << endl;
        return 1;
    }

    string stockNumber = argv[1];
    int tramNumber;

    try {
        tramNumber = stoi(stockNumber);
        if (tramNumber >= 1000 || tramNumber < 0) {
            cerr << "Invalid tram number (must be between 0 and 999)" << endl;
            return 2;
        }
    } catch (const exception& ex) {
        cerr << "Invalid tram number: " << ex.what() << endl;
        return 2;
    }

    int status = 0;
    Ice::CommunicatorPtr ic;

    try {
        ic = Ice::initialize(argc, argv);

        // Configure port for this tram (unique based on stock number)
        stringstream endpoint;
        int port = 9000 + tramNumber;
        endpoint << "default -p " << port;

        Ice::ObjectAdapterPtr adapter = ic->createObjectAdapterWithEndpoints("TramAdapter", endpoint.str());

        TramImpl* tramImpl = new TramImpl(stockNumber);
        Ice::ObjectPtr tramObj = tramImpl;

        string tramIdentity = "Tram" + stockNumber;
        adapter->add(tramObj, Ice::stringToIdentity(tramIdentity));
        adapter->activate();

        cout << "Tram with stock number " << stockNumber << " started on port " << port << endl;

        // Get tram proxy
        TramPrx tramProxy = TramPrx::uncheckedCast(
                adapter->createProxy(Ice::stringToIdentity(tramIdentity))
        );

        // Set self proxy for use in updates
        tramImpl->setSelfProxy(tramProxy);

        // Connect to MPK
        MPKPrx mpkProxy;
        try {
            mpkProxy = MPKPrx::uncheckedCast(ic->stringToProxy("MPK:default -p 10000"));
            cout << "Connected to MPK system" << endl;
        } catch (const exception& ex) {
            cerr << "Failed to connect to MPK system: " << ex.what() << endl;
            return 3;
        }

        // Get available lines
        LineList lines;
        try {
            lines = mpkProxy->getLines();
            if (lines.empty()) {
                cerr << "No lines available in the system" << endl;
                return 4;
            }

            // Connect to first line by default
            LinePrx lineProxy = lines[0];
            tramImpl->setLine(lineProxy);

            try {
                lineProxy->registerTram(tramProxy);
                cout << "Registered on line: " << lineProxy->getName() << endl;
            } catch (const exception& ex) {
                cerr << "Failed to register on line: " << ex.what() << endl;
            }
        } catch (const exception& ex) {
            cerr << "Failed to get lines: " << ex.what() << endl;
            return 5;
        }

        // Connect to depo
        try {
            DepoList depos = mpkProxy->getDepos();
            if (!depos.empty()) {
                DepoPrx depoProxy = depos[0].stop;
                depoProxy->TramOnline(tramProxy);
                cout << "Tram registered at depo: " << depos[0].name << endl;
            }
        } catch (const exception& ex) {
            cerr << "Failed to register at depo: " << ex.what() << endl;
        }

        // Start automatic movement thread
        thread autoThread(autoMoveThread, tramImpl);

        // Command loop
        printHelp();
        string command;

        while (running) {
            cout << "\nEnter command: ";
            getline(cin, command);

            istringstream iss(command);
            string cmd;
            iss >> cmd;

            if (cmd == "exit") {
                running = false;
                cv.notify_all();  // Notify auto thread to check the running flag
                cout << "Shutting down tram..." << endl;
            }
            else if (cmd == "help") {
                printHelp();
            }
            else if (cmd == "status") {
                cout << "\n==== Tram Status ====" << endl;
                cout << "Stock Number: " << tramImpl->getStockNumber() << endl;

                LinePrx line = tramImpl->getLine();
                if (line) {
                    cout << "Line: " << line->getName() << endl;

                    int stopPosition = tramImpl->getCurrentStopPosition();
                    if (stopPosition >= 0) {
                        cout << "Current Stop: " << tramImpl->getCurrentStopName() << endl;

                        if (!tramImpl->isAtTerminus()) {
                            cout << "Next Stop: " << tramImpl->getNextStopName() << endl;
                        } else {
                            cout << "Position: Terminus (End of Line)" << endl;
                        }
                    } else {
                        cout << "Position: Not on route yet" << endl;
                        if (!tramImpl->getNextStops(1).empty()) {
                            cout << "Next Stop: " << tramImpl->getNextStopName() << endl;
                        }
                    }

                    cout << "Auto mode: " << (autoMode ? "Enabled" : "Disabled") << endl;
                } else {
                    cout << "Not assigned to any line" << endl;
                }

                cout << "====================" << endl;
            }
            else if (cmd == "move") {
                if (autoMode) {
                    cout << "Cannot move manually while in auto mode. Please disable auto mode first." << endl;
                    continue;
                }

                if (tramImpl->moveToNextStop()) {
                    cout << "Moved to stop: " << tramImpl->getCurrentStopName() << endl;
                } else {
                    cout << "Failed to move. End of line reached or no line assigned." << endl;
                }
            }
            else if (cmd == "auto") {
                autoMode = !autoMode;
                cout << "Auto mode " << (autoMode ? "enabled" : "disabled") << endl;
                if (autoMode) {
                    cv.notify_one();  // Notify the auto thread to start moving
                }
            }
            else if (cmd == "line") {
                string lineName;
                iss >> lineName;

                if (lineName.empty()) {
                    cout << "Available lines:" << endl;
                    for (const auto& line : lines) {
                        cout << "  - " << line->getName() << endl;
                    }
                    cout << "Usage: line <name>" << endl;
                    continue;
                }

                // Find the requested line
                LinePrx newLine;
                bool found = false;

                for (const auto& line : lines) {
                    if (line->getName() == lineName) {
                        newLine = line;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    cout << "Line '" << lineName << "' not found" << endl;
                    continue;
                }

                // Unregister from current line if needed
                LinePrx currentLine = tramImpl->getLine();
                if (currentLine) {
                    try {
                        currentLine->unregisterTram(tramProxy);
                        cout << "Unregistered from line: " << currentLine->getName() << endl;
                    } catch (const exception& ex) {
                        cerr << "Failed to unregister from current line: " << ex.what() << endl;
                    }
                }

                // Register on new line
                try {
                    tramImpl->setLine(newLine);
                    newLine->registerTram(tramProxy);
                    cout << "Registered on line: " << newLine->getName() << endl;
                } catch (const exception& ex) {
                    cerr << "Failed to register on new line: " << ex.what() << endl;
                }
            }
            else {
                cout << "Unknown command. Type 'help' for available commands." << endl;
            }
        }

        // Cleanup before shutting down
        LinePrx line = tramImpl->getLine();
        if (line) {
            try {
                line->unregisterTram(tramProxy);
                cout << "Unregistered from line" << endl;
            } catch (...) {
                // Ignore exceptions during shutdown
            }
        }

        // Wait for auto thread to finish
        if (autoThread.joinable()) {
            autoThread.join();
        }

        // Clean shutdown
        if (ic) {
            ic->destroy();
        }

    } catch (const exception& ex) {
        cerr << "Error: " << ex.what() << endl;
        status = 1;
    }

    return status;
}