#include "db.h"
#include <iostream>
using namespace std;

int main(){
  if (!connectDB()){
    cerr <<"Could not connect. Exiting.\n";
    return 1;
  }
  pqxx::work txn(getConnection());
  pqxx::result r = txn.exec("SELECT count(*) FROM customers");
  txn.commit();
  cout <<"Customer count: "<< r[0][0].as<int>()<<"\n";
  disconnectDB();
  return 0;
}