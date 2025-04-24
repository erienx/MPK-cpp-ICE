module SIP {
  interface Tram;
  interface TramStop;
  interface Depo;
  interface Passenger;


  struct Time {
    int hour;
    int minute;
  };


  struct StopInfo
  {
     Time time;
     TramStop* stop;
  };
  sequence<StopInfo> StopList;

  struct TramInfo {
     Time time;
     Tram* tram;
  };
  sequence<TramInfo> TramList;

  struct DepoInfo {
     string name;
     Depo* stop;
  };
  sequence<DepoInfo> DepoList;

  interface TramStop {
     string getName();
     TramList getNextTrams(int howMany);
     void RegisterPassenger(Passenger* p);
     void UnregisterPassenger(Passenger* p);
     void UpdateTramInfo(Tram* tram, Time time);
  };

  interface Line
  {
		TramList getTrams();
		StopList getStops();
		void registerTram(Tram* tram);
		void unregisterTram(Tram* tram);
		void setStops(StopList sl);
		string getName();
  };
  sequence<Line*> LineList;


  interface LineFactory {
		Line* createLine(string name);
		double getLoad();
  };

  interface StopFactory {
		TramStop* createStop(string name);
		double getLoad();
  };

  interface MPK {
    TramStop* getTramStop(string name);
    void registerDepo(Depo* depo);
    void unregisterDepo(Depo* depo);
    Depo* getDepo(string name);
    DepoList getDepos();
    LineList getLines();
    void registerLineFactory(LineFactory* lf);
    void unregisterLineFactory(LineFactory* lf);
    void registerStopFactory(StopFactory* lf);
    void unregisterStopFactory(StopFactory* lf);
  };

  interface Depo {
      void TramOnline(Tram* t);
      void TramOffline(Tram* t);
      string getName();
  };

  interface Tram {
    TramStop* getLocation();
    Line* getLine();
    void setLine(Line* line);
    StopList getNextStops(int howMany);
    void RegisterPassenger(Passenger* p);
    void UnregisterPassenger(Passenger* p);
    string getStockNumber();
  };

  interface Passenger
  {
	  void updateTramInfo(Tram* tram, StopList stops);
	  void updateStopInfo(TramStop* stop, TramList trams);
  };
};
