#include <Ice/Ice.h>
#include <SIP.h>
#include <map>
#include <vector>
#include <mutex>

using namespace std;
using namespace SIP;

class TramStopI;
class DepoI;
class LineI;
class MPKImpl;


class MPKImpl : public MPK {
    map<string, TramStopPrx> tramStops;
    map<string, DepoPrx> depos;
    vector<LinePrx> lines;
    vector<LineFactoryPrx> lineFactories;
    vector<StopFactoryPrx> stopFactories;

public:
    virtual TramStopPrx getTramStop(const string& name, const Ice::Current& = Ice::Current()) override {
        auto it = tramStops.find(name);
        if (it != tramStops.end())
            return it->second;
        throw runtime_error("Tram stop not found");
    }

    virtual void registerDepo(const DepoPrx& depo, const Ice::Current& = Ice::Current()) override {
        depos[depo->getName()] = depo;
    }

    virtual void unregisterDepo(const DepoPrx& depo, const Ice::Current& = Ice::Current()) override {
        depos.erase(depo->getName());
    }

    virtual DepoPrx getDepo(const string& name, const Ice::Current& = Ice::Current()) override {
        return depos.at(name);
    }

    virtual DepoList getDepos(const Ice::Current& = Ice::Current()) override {
        DepoList list;
        for (auto& kv : depos) {
            DepoInfo info;
            info.name = kv.first;
            info.stop = kv.second;
            list.push_back(info);
        }
        return list;
    }

    virtual LineList getLines(const Ice::Current& = Ice::Current()) override {
        return lines;
    }

    virtual void registerLineFactory(const LineFactoryPrx &lf, const Ice::Current& = Ice::Current()) override {
        lineFactories.push_back(lf);
    }

    virtual void unregisterLineFactory(const LineFactoryPrx &lf, const Ice::Current& = Ice::Current()) override {
        lineFactories.erase(remove(lineFactories.begin(), lineFactories.end(), lf), lineFactories.end());
    }

    virtual void registerStopFactory(const StopFactoryPrx &sf, const Ice::Current& = Ice::Current()) override {
        stopFactories.push_back(sf);
    }

    virtual void unregisterStopFactory(const StopFactoryPrx &sf, const Ice::Current& = Ice::Current()) override {
        stopFactories.erase(remove(stopFactories.begin(), stopFactories.end(), sf), stopFactories.end());
    }


    void addTramStop(const TramStopPrx &ts) {
        tramStops[ts->getName()] = ts;
    }
    void addLine(const LinePrx &lineProxy) {
        lines.push_back(lineProxy);
    }
};
Ice::ObjectPtr createMPKImpl() {
    return new MPKImpl();
}

class DepoImpl : public Depo {
    string name;
    set<TramPrx> onlineTrams;
public:
    DepoImpl(const string &n) : name(n) {}
    virtual void TramOnline(const TramPrx &t, const Ice::Current& = Ice::Current()) override {
        onlineTrams.insert(t);
        cout << "tram " << t->getStockNumber() << " is online at " << name << "depo" <<  endl;
    }
    virtual void TramOffline(const TramPrx &t, const Ice::Current& = Ice::Current()) override {
        onlineTrams.erase(t);
        cout << "tram " << t->getStockNumber() << " is offline at " << name << "depo" << endl;
    }
    virtual string getName(const Ice::Current& = Ice::Current()) override {
        return name;
    }
};

Ice::ObjectPtr createDepoImpl(const string &name) {
    return new DepoImpl(name);
}


class LineImpl : public Line {
    string name;
    TramList trams;
    StopList stops;
public:
    LineImpl(const string &n) : name(n) {}

    virtual TramList getTrams(const Ice::Current& = Ice::Current()) override {
        return trams;
    }
    virtual StopList getStops(const Ice::Current& = Ice::Current()) override {
        return stops;
    }
    virtual void registerTram(const TramPrx &tram, const Ice::Current& = Ice::Current()) override {
        TramInfo info;
        info.time.hour = 0;
        info.time.minute = 0;
        info.tram = tram;
        trams.push_back(info);
        cout << "registered tram " << tram->getStockNumber() << " on line " << name << endl;
    }
    virtual void unregisterTram(const TramPrx &tram, const Ice::Current& = Ice::Current()) override {
        auto it = remove_if(trams.begin(), trams.end(), [tram](const TramInfo &info) {
            return info.tram == tram;
        });
        trams.erase(it, trams.end());
        cout << "unregistered tram " << tram->getStockNumber() << " from line " << name << endl;
    }
    virtual void setStops(const StopList &sl, const Ice::Current& = Ice::Current()) override {
        stops = sl;
        cout << "added stops for line  " << name << endl;
    }
    virtual string getName(const Ice::Current& = Ice::Current()) override {
        return name;
    }
};

Ice::ObjectPtr createLineImpl(const string &name) {
    return new LineImpl(name);
}

class TramStopImpl : public TramStop {
    string name;
    set<PassengerPrx> passengers;
    TramList upcomingTrams;
    TramStopPrx selfProxy;
public:
    TramStopImpl(const string &n) : name(n) {}
    void setSelfProxy(const TramStopPrx &proxy) {
        selfProxy = proxy;
    }
    virtual string getName(const Ice::Current& = Ice::Current()) override {
        return name;
    }
    virtual TramList getNextTrams(int howMany, const Ice::Current& = Ice::Current()) override {
        TramList result;
        int count = 0;
        for(const auto &ti : upcomingTrams) {
            if(count >= howMany)
                break;
            result.push_back(ti);
            count++;
        }
        return result;
    }
    virtual void RegisterPassenger(const PassengerPrx &p, const Ice::Current& = Ice::Current()) override {
        passengers.insert(p);
        cout << "passenger registered at stop " << name << endl;
    }
    virtual void UnregisterPassenger(const PassengerPrx &p, const Ice::Current& = Ice::Current()) override {
        passengers.erase(p);
        cout << "passenger unregistered at stop " << name << endl;
    }
    virtual void UpdateTramInfo(const TramPrx &tram, const Time& time, const Ice::Current& = Ice::Current()) override {
        TramInfo info;
        info.time = time;
        info.tram = tram;
        upcomingTrams.push_back(info);
        cout << "tram info updated at stop " << name << " for tram " << tram->getStockNumber() << ", " <<  time.hour << ":" << time.minute << endl;
        for (auto &p : passengers) {
            try {
                if(selfProxy) {
                    p->updateStopInfo(selfProxy, upcomingTrams);
                }
            } catch (const exception &ex) {
                cerr << "callback err : " << ex.what() << endl;
            }
        }
    }
};

Ice::ObjectPtr createTramStopImpl(const string &name) {
    return new TramStopImpl(name);
}

class LineFactoryImpl : public LineFactory {
    Ice::ObjectAdapterPtr adapter;
    map<string, LinePrx> lines;
    mutable std::mutex mtx;

public:
    LineFactoryImpl(const Ice::ObjectAdapterPtr& adapter) : adapter(adapter) {}

    virtual LinePrx createLine(const string& name, const Ice::Current&) override {
        std::lock_guard<std::mutex> lock(mtx);
        if (lines.find(name) != lines.end()) {
            return lines[name];
        }

        Ice::ObjectPtr lineImpl = new LineImpl(name);
        Ice::Identity id = Ice::stringToIdentity(name);
        adapter->add(lineImpl, id);
        LinePrx lineProxy = LinePrx::uncheckedCast(adapter->createProxy(id));
        lines[name] = lineProxy;
        return lineProxy;
    }

    virtual double getLoad(const Ice::Current&) override {
        std::lock_guard<std::mutex> lock(mtx);
        return static_cast<double>(lines.size());
    }
};
void printHelp(){
    cout << "\n==== MPK Information System ====" << endl;
    cout << "Available commands:" << endl;
    cout << "  info         - Show general system information" << endl;
    cout << "  lines        - List all lines" << endl;
    cout << "  line <name>  - Show details about specific line" << endl;
    cout << "  stops        - List all tram stops" << endl;
    cout << "  stop <name>  - Show information about specific stop" << endl;
    cout << "  depos        - List all depos" << endl;
    cout << "  help         - Show this help message" << endl;
    cout << "  exit         - Exit the program" << endl;
    cout << "=============================" << endl;
}
int main(int argc, char* argv[]) {
    int status = 0;
    Ice::CommunicatorPtr ic;

    try {
        ic = Ice::initialize(argc, argv);

        Ice::ObjectAdapterPtr mpkAdapter = ic->createObjectAdapterWithEndpoints("MPKAdapter", "default -p 10000");
        Ice::ObjectAdapterPtr depoAdapter = ic->createObjectAdapterWithEndpoints("DepoAdapter", "default -p 10003");
        Ice::ObjectAdapterPtr lineAdapter = ic->createObjectAdapterWithEndpoints("LineAdapter", "default -p 10004");
        Ice::ObjectAdapterPtr stopAdapter = ic->createObjectAdapterWithEndpoints("StopAdapter", "default -p 10007");
        Ice::ObjectAdapterPtr factoryAdapter = ic->createObjectAdapterWithEndpoints("FactoryAdapter", "default -p 10008");
        mpkAdapter->activate();
        depoAdapter->activate();
        lineAdapter->activate();
        stopAdapter->activate();
        factoryAdapter->activate();

        MPKImpl* mpkImpl = new MPKImpl();
        mpkAdapter->add(mpkImpl, Ice::stringToIdentity("MPK"));
        MPKPrx mpkProxy = MPKPrx::uncheckedCast(mpkAdapter->createProxy(Ice::stringToIdentity("MPK")));

        Ice::ObjectPtr lineFactory = new LineFactoryImpl(lineAdapter);
        factoryAdapter->add(lineFactory, Ice::stringToIdentity("LineFactory"));
        LineFactoryPrx lineFactoryProxy = LineFactoryPrx::uncheckedCast(
                factoryAdapter->createProxy(Ice::stringToIdentity("LineFactory"))
        );
        mpkProxy->registerLineFactory(lineFactoryProxy);

        Ice::ObjectPtr stopA = new TramStopImpl("StopA");
        Ice::ObjectPtr stopB = new TramStopImpl("StopB");
        Ice::ObjectPtr stopC = new TramStopImpl("StopC");
        stopAdapter->add(stopA, Ice::stringToIdentity("StopA"));
        stopAdapter->add(stopB, Ice::stringToIdentity("StopB"));
        stopAdapter->add(stopC, Ice::stringToIdentity("StopC"));

        TramStopPrx stopAProxy = TramStopPrx::uncheckedCast(stopAdapter->createProxy(Ice::stringToIdentity("StopA")));
        TramStopPrx stopBProxy = TramStopPrx::uncheckedCast(stopAdapter->createProxy(Ice::stringToIdentity("StopB")));
        TramStopPrx stopCProxy = TramStopPrx::uncheckedCast(stopAdapter->createProxy(Ice::stringToIdentity("StopC")));

        // Fix missing setSelfProxy calls
        dynamic_cast<TramStopImpl*>(stopA.get())->setSelfProxy(stopAProxy);
        dynamic_cast<TramStopImpl*>(stopB.get())->setSelfProxy(stopBProxy);
        dynamic_cast<TramStopImpl*>(stopC.get())->setSelfProxy(stopCProxy);

        mpkImpl->addTramStop(stopAProxy);
        mpkImpl->addTramStop(stopBProxy);
        mpkImpl->addTramStop(stopCProxy);

        Ice::ObjectPtr depo = new DepoImpl("Depo1");
        depoAdapter->add(depo, Ice::stringToIdentity("Depo1"));
        DepoPrx depoProxy = DepoPrx::uncheckedCast(depoAdapter->createProxy(Ice::stringToIdentity("Depo1")));
        mpkProxy->registerDepo(depoProxy);

        LinePrx line1Proxy = lineFactoryProxy->createLine("Line1");
        mpkImpl->addLine(line1Proxy);

        StopList stopList;
        stopList.push_back(StopInfo{Time{0, 0}, stopAProxy});
        stopList.push_back(StopInfo{Time{0, 10}, stopBProxy});
        stopList.push_back(StopInfo{Time{0, 20}, stopCProxy});
        line1Proxy->setStops(stopList);

        // Create a second line for demonstration
        LinePrx line2Proxy = lineFactoryProxy->createLine("Line2");
        mpkImpl->addLine(line2Proxy);

        StopList stopList2;
        stopList2.push_back(StopInfo{Time{0, 0}, stopCProxy});
        stopList2.push_back(StopInfo{Time{0, 15}, stopBProxy});
        line2Proxy->setStops(stopList2);

        cout << "System initialized" << endl;

        // Interactive console loop
        bool running = true;
        string command;

        printHelp();

        while (running) {
            cout << "\nEnter command: ";
            getline(cin, command);

            // Parse command
            istringstream iss(command);
            string cmd;
            iss >> cmd;

            if (cmd == "exit") {
                running = false;
                cout << "Shutting down..." << endl;
            }
            else if (cmd == "help") {
                printHelp();
            }
            else if (cmd == "info") {
                cout << "\n==== MPK System Information ====" << endl;

                // Count lines
                LineList lines = mpkProxy->getLines();
                cout << "Total lines: " << lines.size() << endl;

                // Count stops (this is from our local knowledge)
                cout << "Total stops: " << 3 << endl;

                // Count depos
                DepoList depos = mpkProxy->getDepos();
                cout << "Total depos: " << depos.size() << endl;

                cout << "=============================" << endl;
            }
            else if (cmd == "lines") {
                LineList lines = mpkProxy->getLines();
                cout << "\n==== Available Lines ====" << endl;

                if (lines.empty()) {
                    cout << "No lines available." << endl;
                }
                else {
                    for (const auto& line : lines) {
                        cout << "Line: " << line->getName() << endl;
                    }
                }

                cout << "========================" << endl;
            }
            else if (cmd == "line") {
                string lineName;
                iss >> lineName;

                if (lineName.empty()) {
                    cout << "Please specify a line name." << endl;
                    continue;
                }

                // Find the line
                LineList lines = mpkProxy->getLines();
                LinePrx foundLine;
                bool found = false;

                for (const auto& line : lines) {
                    if (line->getName() == lineName) {
                        foundLine = line;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    cout << "Line '" << lineName << "' not found." << endl;
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
                }
                else {
                    for (const auto& tram : trams) {
                        cout << "  - Stock Number: " << tram.tram->getStockNumber() << endl;
                    }
                }

                cout << "=============================" << endl;
            }
            else if (cmd == "stops") {
                cout << "\n==== Available Tram Stops ====" << endl;
                cout << "StopA" << endl;
                cout << "StopB" << endl;
                cout << "StopC" << endl;
                cout << "=============================" << endl;
            }
            else if (cmd == "stop") {
                string stopName;
                iss >> stopName;

                if (stopName.empty()) {
                    cout << "Please specify a stop name." << endl;
                    continue;
                }

                try {
                    // Try to get the stop
                    TramStopPrx stop = mpkProxy->getTramStop(stopName);

                    cout << "\n==== Tram Stop " << stop->getName() << " Information ====" << endl;

                    // Get next trams
                    TramList nextTrams = stop->getNextTrams(5);
                    cout << "Next trams (" << nextTrams.size() << "):" << endl;

                    if (nextTrams.empty()) {
                        cout << "  No upcoming trams." << endl;
                    }
                    else {
                        for (const auto& tram : nextTrams) {
                            cout << "  - Tram " << tram.tram->getStockNumber();
                            cout << " (Arrival: " << tram.time.hour << ":"
                                 << (tram.time.minute < 10 ? "0" : "") << tram.time.minute << ")" << endl;
                        }
                    }

                    cout << "=============================" << endl;
                }
                catch (const exception& ex) {
                    cout << "Error: " << ex.what() << endl;
                }
            }
            else if (cmd == "depos") {
                DepoList depos = mpkProxy->getDepos();
                cout << "\n==== Available Depos ====" << endl;

                if (depos.empty()) {
                    cout << "No depos available." << endl;
                }
                else {
                    for (const auto& depo : depos) {
                        cout << "Depo: " << depo.name << endl;
                    }
                }

                cout << "=========================" << endl;
            }
            else {
                cout << "Unknown command. Type 'help' for available commands." << endl;
            }
        }

        if (ic) ic->destroy();
    } catch (const Ice::Exception& ex) {
        cerr << ex << endl;
        status = 1;
    }

    return status;
}