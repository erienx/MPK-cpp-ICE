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

        if (!upcomingTrams.empty() && selfProxy) {
            try {
                p->updateStopInfo(selfProxy, upcomingTrams);
            } catch (const exception &ex) {
                cerr << "error sending info: " << endl;
            }
        }
    }

    virtual void UnregisterPassenger(const PassengerPrx &p, const Ice::Current& = Ice::Current()) override {
        passengers.erase(p);
        cout << "passenger unregistered at stop " << name << endl;
    }

    virtual void UpdateTramInfo(const TramPrx &tram, const Time& time, const Ice::Current& = Ice::Current()) override {
        bool tramFound = false;
        for (auto &ti : upcomingTrams) {
            if (ti.tram == tram) {
                ti.time = time;
                tramFound = true;
                break;
            }
        }

        if (!tramFound) {
            TramInfo info;
            info.time = time;
            info.tram = tram;
            upcomingTrams.push_back(info);
        }

        sort(upcomingTrams.begin(), upcomingTrams.end(),
             [](const TramInfo &a, const TramInfo &b) {
                 if (a.time.hour != b.time.hour)
                     return a.time.hour < b.time.hour;
                 return a.time.minute < b.time.minute;
             });

        Time currentTime;
        auto now = chrono::system_clock::now();
        time_t currentTimeT = chrono::system_clock::to_time_t(now);
        tm* timeinfo = localtime(&currentTimeT);
        currentTime.hour = timeinfo->tm_hour;
        currentTime.minute = timeinfo->tm_min;

        upcomingTrams.erase(
                remove_if(upcomingTrams.begin(), upcomingTrams.end(),
                          [&currentTime](const TramInfo &ti) {
                              return (ti.time.hour < currentTime.hour) ||
                                     (ti.time.hour == currentTime.hour && ti.time.minute < currentTime.minute);
                          }),
                upcomingTrams.end());

        for (const auto &p : passengers) {
            if (selfProxy) {
                p->updateStopInfo(selfProxy, upcomingTrams);
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

//class StopFactoryImpl : public StopFactory {
//    Ice::ObjectAdapterPtr adapter;
//    map<string, TramStopPrx> stops;
//    mutable std::mutex mtx;
//
//public:
//    StopFactoryImpl(const Ice::ObjectAdapterPtr& adapter) : adapter(adapter) {}
//
//    virtual TramStopPrx createStop(const string& name, const Ice::Current&) override {
//        std::lock_guard<std::mutex> lock(mtx);
//        if (stops.find(name) != stops.end()) {
//            return stops[name];
//        }
//
//        Ice::ObjectPtr stopImpl = new TramStopImpl(name);
//        Ice::Identity id = Ice::stringToIdentity(name);
//        adapter->add(stopImpl, id);
//        TramStopPrx stopProxy = TramStopPrx::uncheckedCast(adapter->createProxy(id));
//
//        dynamic_cast<TramStopImpl*>(stopImpl.get())->setSelfProxy(stopProxy);
//
//        stops[name] = stopProxy;
//        cout << "created new tram stop: " << name << endl;
//        return stopProxy;
//    }
//
//    virtual double getLoad(const Ice::Current&) override {
//        std::lock_guard<std::mutex> lock(mtx);
//        return static_cast<double>(stops.size());
//    }
//};

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

//        Ice::ObjectPtr stopFactory = new StopFactoryImpl(stopAdapter);
//        factoryAdapter->add(stopFactory, Ice::stringToIdentity("StopFactory"));
        StopFactoryPrx stopFactoryProxy = StopFactoryPrx::uncheckedCast(
            factoryAdapter->createProxy(Ice::stringToIdentity("StopFactory"))
        );
        mpkProxy->registerStopFactory(stopFactoryProxy);

        Ice::ObjectPtr stopA = new TramStopImpl("StopA");
        Ice::ObjectPtr stopB = new TramStopImpl("StopB");
        Ice::ObjectPtr stopC = new TramStopImpl("StopC");
        stopAdapter->add(stopA, Ice::stringToIdentity("StopA"));
        stopAdapter->add(stopB, Ice::stringToIdentity("StopB"));
        stopAdapter->add(stopC, Ice::stringToIdentity("StopC"));

        TramStopPrx stopAProxy = TramStopPrx::uncheckedCast(stopAdapter->createProxy(Ice::stringToIdentity("StopA")));
        TramStopPrx stopBProxy = TramStopPrx::uncheckedCast(stopAdapter->createProxy(Ice::stringToIdentity("StopB")));
        TramStopPrx stopCProxy = TramStopPrx::uncheckedCast(stopAdapter->createProxy(Ice::stringToIdentity("StopC")));

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

        LinePrx line2Proxy = lineFactoryProxy->createLine("Line2");
        mpkImpl->addLine(line2Proxy);

        StopList stopList2;
        stopList2.push_back(StopInfo{Time{0, 0}, stopCProxy});
        stopList2.push_back(StopInfo{Time{0, 15}, stopBProxy});
        line2Proxy->setStops(stopList2);

        cout << "running...\n" << endl;

        bool running = true;
        string command;

        cout << "commands:" << endl;
        cout << "lines        - list lines" << endl;
        cout << "line <name>  - details about a line" << endl;
        cout << "stop <name>  - details about a stop" << endl;
        cout << "depos        - list deops" << endl;
        cout << "exit         - exit" << endl;

        while (running) {
            cout << "\nEnter command: ";
            getline(cin, command);

            istringstream iss(command);
            string cmd;
            iss >> cmd;

            if (cmd == "exit") {
                running = false;
                cout << "closing..." << endl;
            }
            else if (cmd == "lines") {
                LineList lines = mpkProxy->getLines();
                cout << "\nLines:" << endl;

                if (lines.empty()) {
                    cout << "no lines" << endl;
                }
                else {
                    for (const auto& line : lines) {
                        cout << "line: " << line->getName() << endl;
                    }
                }
            }
            else if (cmd == "line") {
                string lineName;
                iss >> lineName;

                if (lineName.empty()) {
                    cout << "no line name provided. skip.." << endl;
                    continue;
                }

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
                    cout << "line not found" << endl;
                    continue;
                }

                StopList stops = foundLine->getStops();
                for (const auto& stop : stops) {
                    cout << "- " << stop.stop->getName() <<" (Arrival time: " << stop.time.hour << ":" << stop.time.minute << ")" << endl;
                }

                cout << "\ntrams" << endl;
                TramList trams = foundLine->getTrams();
                if (trams.empty()) {
                    cout << "no trams on this line" << endl;
                }
                else {
                    for (const auto& tram : trams) {
                        cout << "- tram number: " << tram.tram->getStockNumber() << endl;
                    }
                }
            }
            else if (cmd == "stop") {
                string stopName;
                iss >> stopName;

                if (stopName.empty()) {
                    cout << "no stop name provided.. skip" << endl;
                    continue;
                }

                try {
                    TramStopPrx stop = mpkProxy->getTramStop(stopName);
                    TramList nextTrams = stop->getNextTrams(5);
                    if (nextTrams.empty()) {
                        cout << "no upcoming trams" << endl;
                    }
                    else {
                        for (const auto& tram : nextTrams) {
                            cout << "- tram " << tram.tram->getStockNumber() << " (Arrival: "
                                 << tram.time.hour << ":" << tram.time.minute<< ")" << endl;
                        }
                    }
                } catch (const std::exception& ex) {
                    cout << "error getting stop info: " << endl;
                }
            }
            else if (cmd == "depos") {
                DepoList depos = mpkProxy->getDepos();

                if (depos.empty()) {
                    cout << "no depos available" << endl;
                }
                else {
                    for (const auto& depo : depos) {
                        cout << "depo: " << depo.name << endl;
                    }
                }
            }
            else {
                cout << "unknown command" << endl;
            }
        }
        if (ic) ic->destroy();
    } catch (const Ice::Exception& ex) {
        cerr << ex << endl;
        status = 1;
    }

    return status;
}