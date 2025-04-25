#include <Ice/Ice.h>
#include <SIP.h>
#include <iostream>

using namespace std;
using namespace SIP;

class PassengerImpl : public Passenger {
public:
    virtual void updateTramInfo(const TramPrx& tram, const StopList& stops, const Ice::Current& = Ice::Current()) override {
        cout << "info for tram" << tram->getStockNumber() << ":\n";
        for (const auto& stop : stops) {
            cout << "**at time " << stop.time.hour << ":" << stop.time.minute << endl;
        }
    }

    virtual void updateStopInfo(const TramStopPrx& stop, const TramList& trams, const Ice::Current& = Ice::Current()) override {
        cout << "list of trams approaching stop " << endl;
        for (const auto& tram : trams) {
            cout << "**tram at " << tram.time.hour << ":" << tram.time.minute << endl;
        }
    }
};

int main(int argc, char* argv[]) {
    int status = 0;
    Ice::CommunicatorPtr ic;

    try {
        ic = Ice::initialize(argc, argv);
        Ice::ObjectAdapterPtr adapter =
                ic->createObjectAdapterWithEndpoints("ClientAdapter", "default -p 10002");

        Ice::ObjectPtr passenger = new PassengerImpl();
        adapter->add(passenger, Ice::stringToIdentity("Passenger1"));

        adapter->activate();
        cout << "client is on.." << endl;


        Ice::ObjectPrx base = ic->stringToProxy("MPK:default -p 10000");
        MPKPrx mpk = MPKPrx::checkedCast(base);
        if(!mpk) {
            cerr << "incorrect mpk proxy" << endl;
        } else {

            DepoList depoList = mpk->getDepos();
            cout << "depo list:" << endl;
            for(const auto &dInfo : depoList) {
                cout << "**depo: " << dInfo.name << endl;
            }

            try {
                TramStopPrx ts = mpk->getTramStop("StopCentral");
                cout << "found stop: " << ts->getName() << endl;
            } catch (const exception &ex) {
                cout << "couldnt get tram stop, msg: : " << ex.what() << endl;
            }

            LineList lines = mpk->getLines();
            cout << "lines count: " << lines.size() << endl;
            for(const auto &line : lines) {
                cout << "**line: " << line->getName() << endl;
                TramList tramList = line->getTrams();
                cout << "tram list: " << tramList.size() << endl;
                for(const auto &tramInfo : tramList) {
                    cout << " **tram: " << tramInfo.tram->getStockNumber() << " time: "
                         << tramInfo.time.hour << ":" << tramInfo.time.minute << endl;
                }
            }
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
