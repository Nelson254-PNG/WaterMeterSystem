#include "db.h"
#include <memory>
#include <iostream>

using namespace std;

static unique_ptr<pqxx::connection> conn;

static const string CONNECTION_STRING = "dbname=watermeter_system user=postgres password=admin123 host=127.0.0.1 port=5432";

bool connectDB(){
  try{
    conn = make_unique<pqxx::connection>(CONNECTION_STRING);
    
    if(conn->is_open()){
      cout <<"Connected to database: "<< conn->dbname() <<"\n";
      return true;
    }else{
      cerr << "Connection object created but not open.\n";
      return false;
    }
  }catch (const exception& e){
    cerr<<"Database connection failed: "<<e.what()<<"\n";
    return false;
  }
}
pqxx::connection& getConnection(){
  if(!conn){
    throw runtime_error("getConnection() called before connectDB() succeeded");
  }
  return *conn;
}
void disconnectDB(){
  if (conn){
    conn->close();
    cout <<"Database connection closed.\n";

  }
}
