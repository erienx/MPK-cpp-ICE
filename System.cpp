#include <Ice/Ice.h>
#include <SIP.h>
#include <map>
#include <vector>

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
public:
    TramStopImpl(const string &n) : name(n) {}
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
        cout << "tram info updated at stop " << name << " for tram " << tram->getStockNumber() << endl;
    }
};

Ice::ObjectPtr createTramStopImpl(const string &name) {
    return new TramStopImpl(name);
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

        Ice::ObjectPtr mpkObj = createMPKImpl();
        mpkAdapter->add(mpkObj, Ice::stringToIdentity("MPK"));

        Ice::ObjectPtr depoObj = createDepoImpl("CentralDepo");
        depoAdapter->add(depoObj, Ice::stringToIdentity("DepoCentral"));

        Ice::ObjectPtr lineObj = createLineImpl("Line1");
        lineAdapter->add(lineObj, Ice::stringToIdentity("LineLine1"));

        Ice::ObjectPtr stopObj = createTramStopImpl("StopCentral");
        stopAdapter->add(stopObj, Ice::stringToIdentity("StopCentral"));

        mpkAdapter->activate();
        depoAdapter->activate();
        lineAdapter->activate();
        stopAdapter->activate();

        MPKImpl* mpkImpl = dynamic_cast<MPKImpl*>(mpkObj.get());
        if(mpkImpl) {
            TramStopPrx tsProxy = TramStopPrx::uncheckedCast(ic->stringToProxy("StopCentral:default -p 10007"));
            mpkImpl->addTramStop(tsProxy);
            LinePrx lineProxy = LinePrx::uncheckedCast(ic->stringToProxy("LineLine1:default -p 10004"));
            mpkImpl->addLine(lineProxy);
            DepoPrx depoProxy = DepoPrx::uncheckedCast(ic->stringToProxy("DepoCentral:default -p 10003"));
            mpkImpl->registerDepo(depoProxy);
        }

        cout << "system working " << endl;

        ic->waitForShutdown();

    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
        status = 1;
    }

    if (ic) {
        try {
            ic->destroy();
        } catch (const exception& e) {
            cerr << e.what() << endl;
            status = 1;
        }
    }

    return status;
}
