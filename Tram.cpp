#include <Ice/Ice.h>
#include <SIP.h>
#include <iostream>
#include <vector>

using namespace std;
using namespace SIP;

class TramImpl : public Tram {
    string stockNumber;
    TramStopPrx location;
    LinePrx line;
    vector<PassengerPrx> passengers;

public:
    TramImpl(const string& sn) : stockNumber(sn) {}

    virtual TramStopPrx getLocation(const Ice::Current& = Ice::Current()) override {
        return location;
    }

    virtual LinePrx getLine(const Ice::Current& = Ice::Current()) override {
        return line;
    }

    virtual void setLine(const LinePrx& l, const Ice::Current& = Ice::Current()) override {
        line = l;
    }

    virtual StopList getNextStops(int howMany, const Ice::Current& = Ice::Current()) override {
        if (!line)
            return {};
        StopList allStops = line->getStops();
        int counter = 0;
        StopList out;
        for (const auto &stop: allStops){
            if (counter>howMany)
                break;
            out.push_back(stop);
        }
        return out;
    }

    virtual void RegisterPassenger(const PassengerPrx& p, const Ice::Current& = Ice::Current()) override {
        passengers.push_back(p);
    }

    virtual void UnregisterPassenger(const PassengerPrx& p, const Ice::Current& = Ice::Current()) override {
        passengers.erase(remove(passengers.begin(), passengers.end(), p), passengers.end());
    }

    virtual string getStockNumber(const Ice::Current& = Ice::Current()) override {
        return stockNumber;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "no stock number" << endl;
        return 1;
    }

    int status = 0;
    Ice::CommunicatorPtr ic;
    try {
        ic = Ice::initialize(argc, argv);
        Ice::ObjectAdapterPtr adapter = ic->createObjectAdapterWithEndpoints("TramAdapter", "default -p 10001");

        Ice::ObjectPtr tram = new TramImpl(argv[1]);
        adapter->add(tram, Ice::stringToIdentity("Tram" + string(argv[1])));

        adapter->activate();
        cout << "tram wiht number" << argv[1] << " started" << endl;

        TramPrx tramProxy = TramPrx::uncheckedCast(ic->stringToProxy("Tram" + string(argv[1]) + ":default -p 10001"));
        LinePrx lineProxy = LinePrx::uncheckedCast(ic->stringToProxy("LineLine1:default -p 10004"));
        try {
            lineProxy->registerTram(tramProxy);
            cout << "registered on line" << endl;
        } catch(const exception &e) {
            cerr << "err line register: " << e.what() << endl;
        }
        DepoPrx depoProxy = DepoPrx::uncheckedCast(ic->stringToProxy("DepoCentral:default -p 10003"));
        try {
            depoProxy->TramOnline(tramProxy);
            cout << "online in depo" << endl;
        } catch(const exception &e) {
            cerr << "err in depo " << e.what() << endl;
        }


        ic->waitForShutdown();

    } catch (const exception& e) {
        cerr << e.what() << endl;
        status = 1;
    }
    if (ic)
        ic->destroy();
    return status;
}