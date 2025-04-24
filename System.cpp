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
};

int main(int argc, char* argv[]) {
    int status = 0;
    Ice::CommunicatorPtr ic;

    try {
        ic = Ice::initialize(argc, argv);

        Ice::ObjectAdapterPtr adapter = ic->createObjectAdapterWithEndpoints("SystemAdapter", "default -p 10000");

        Ice::ObjectPtr mpkObject = new MPKImpl();
        adapter->add(mpkObject, Ice::stringToIdentity("MPK"));

        adapter->activate();
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
