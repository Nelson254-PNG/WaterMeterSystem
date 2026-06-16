#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <limits>

using namespace std;

const double SERVICE_CHARGE = 200.0;
//limits
const double TIER1_LIMIT = 6.0;
const double TIER2_LIMIT = 20.0;
const double TIER3_LIMIT = 50.0;
//rates
const double TIER1_RATE = 50.0;
const double TIER2_RATE = 75.0;
const double TIER3_RATE = 100.0;
const double TIER4_RATE = 150.0;

struct WaterRecord {
  int recordId;
  string date;
  double currentReading;
  double previousReading;
  double unitsUsed;
  bool billed;
};
//BILLS
struct Bill {
  int billId;
  string issueDate;
  string dueDate;
  double totalUnits;
  double tier1Units;
  double tier2Units;
  double tier3Units;
  double tier4Units;
  double tier1Cost;
  double tier2Cost;
  double tier3Cost;
  double tier4Cost;
  double serviceCharge;
  double totalAmount;
  double amountPaid;
  bool paid;
};
struct Payment{
  int paymentId;
  int billId;
  string date;
  string method;
  string reference;
  double amountPaid;
  double balanceBefore;
  double balanceAfter;
};

struct Customer{
  int id;
  string name;
  string meterNumber;
  string phone;
  double balance;
  double lastReading;
  vector<WaterRecord> records;
  vector<Bill> bills;
  vector<Payment> payments;
};

vector<Customer> customers;

string generateMeterNumber(int id) {
  string padded = to_string(id);
  while (padded.length() < 3) {
    padded = "0" + padded;
  }
  return "MTR-" + padded;
}
//returns a pointer to the found customer or nullptr if not found
Customer* findCustomerById(int id) {
  for (auto& c : customers){
    if (c.id == id) {
      return &c;
    }
  }
  return nullptr;
}
//biiling engine
void calculateTieredCost(double units, Bill& bill){
  bill.tier1Units = 0;
  bill.tier2Units = 0;
  bill.tier3Units = 0;
  bill.tier4Units = 0;

  double remaining = units;
  //0 - 6 m^3 at kes 50
  if (remaining > 0){
    bill.tier1Units = min(remaining, TIER1_LIMIT);
    remaining -= bill.tier1Units;
  }
  //7 - 20 at kes 75
  if (remaining > 0){
    double tier2Capacity = TIER2_LIMIT - TIER1_LIMIT;
    bill.tier2Units = min(remaining, tier2Capacity);
    remaining -= bill.tier2Units;
  }
  //21 - 50 at kes 100
  if (remaining > 0){
    double tier3Capacity = TIER3_LIMIT - TIER2_LIMIT;
    bill.tier3Units = min(remaining, tier3Capacity);
    remaining -= bill.tier3Units;
  }
  //51+ at kes 150
  if (remaining > 0){
    bill.tier4Units = remaining;
  }

  //calculate cost per tier
  bill.tier1Cost = bill.tier1Units * TIER1_RATE;
  bill.tier2Cost = bill.tier2Units * TIER2_RATE;
  bill.tier3Cost = bill.tier3Units * TIER3_RATE;
  bill.tier4Cost = bill.tier3Units * TIER4_RATE;

  //fixed charge
  bill.serviceCharge = SERVICE_CHARGE;
  
  //total
  bill.totalAmount = bill.tier1Cost + bill.tier2Cost + bill.tier3Cost + bill.tier4Cost + bill.serviceCharge;

}
//payments
void makePayment(){
  cout <<"\n Make Payment \n";
  
  if (customers.empty()) { cout <<"No customers registered yet.\n"; return;}
  int id;
  cout <<"Enter Customer ID: ";
  cin >>id;
  Customer* c = findCustomerById(id);
  if (!c) { cout <<"Customer not found.\n"; return; }
  cout <<"\n Customer: "<< c->name <<"\n";
  cout <<" Meter No. : "<< c->meterNumber <<"\n";
  cout <<" Balance: KES"<< fixed <<setprecision(2)<< c->balance <<"\n";

  cout <<"\n Unpaid Bills: \n";
  cout <<" "<< left << setw(8) <<"Bill #"
       << setw(14) << "Issued" << setw(14) <<"Due"
       << setw(14) << "Total(KES)" << setw(14)<<"Paid(KES)"
       << setw (14) << "Remaining(KES)" <<"\n";
  cout <<" " << string(78, '-') <<"\n";  
  
  bool hasUnpaid = false;
  for (const auto& b : c->bills){
    if (!b.paid){
      double remaining = b.totalAmount - b.amountPaid;
      cout <<" "<< left << setw(8) <<b.billId
           << setw(14) << b.issueDate
           << setw(14) << b.dueDate
           << setw(14) << fixed << setprecision(2) <<b.totalAmount
           << setw(14) << b.amountPaid
           << setw(14) << remaining <<"\n";
      hasUnpaid = true;     
    }
  }
  
  if (!hasUnpaid) {
    cout << "No unpaid Bills. Account is clear!\n"; return;

  }

  int billId;
  cout <<"\n Enter Bill # to pay: ";
  cin >> billId;

  Bill* targetBill = nullptr;
  for(auto& b : c->bills){
    if (b.billId == billId && !b.paid){
      targetBill = &b; break;
    }
  }
  if (!targetBill){
    cout << " Bill not found or already paid \n"; return;
  }
  double remaining = targetBill->totalAmount - targetBill->amountPaid;
  cout <<"\n Amount Remaining on Bill #" << billId << ": KES" <<fixed <<setprecision(2) << remaining <<"\n";

  double amount;
  cout <<"Amount to  pay(KES) : ";
  cin >>amount;

  if (amount <=0){
    cout <<" Payment amount must be greator than zero.\n"; return;}
  cout <<"\n Payment Method\n";
  cout <<" 1.Cash\n";
  cout <<" 2.M-pesa\n";
  cout <<" 3.Bank Transfer\n";
  cout <<" Choose: ";
  int methodChoice;
  cin >> methodChoice;

  string method;
  switch (methodChoice){
    case 1: method = "Cash"; break;
    case 2: method = "M-pesa"; break;
    case 3: method = "Bank Transfer"; break;
    default: method = "Other"; break;
  }
  //get reference number
  string reference;
  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  if (method != "Cash"){
    cout <<" Transaction Reference: ";
    getline(cin, reference);
  }else {
    reference = "N/A";
  }

  string payDate;
  cout <<"Payments Date (YYYY-MM-DD): ";
  getline(cin, payDate);

  Payment p;
  p.paymentId = c->payments.size() + 1;
  p.billId = billId;
  p.date = payDate;
  p.method = method;
  p.reference = reference;
  p.amountPaid = amount;
  p.balanceBefore = c->balance;

  targetBill->amountPaid += amount;

  if (targetBill->amountPaid >= targetBill->totalAmount - 0.01){
    targetBill->paid = true;
  }
  c->balance -= amount;
  p.balanceAfter = c->balance;
  c->payments.push_back(p);

  // print receipt
  cout <<"\n";
  cout <<" PAYMENT RECEIPT \n";
  cout <<" Customer : "<< left <<setw(26) <<c->name <<"\n";
  cout <<" Meter No : "<< left <<setw(26) <<c->meterNumber <<"\n";
  cout <<" Payment # : "<< left <<setw(26) <<p.paymentId <<"\n";
  cout <<" Date : "<< left <<setw(26) <<payDate <<"\n";
  cout <<" Method : "<< left <<setw(26) <<method <<"\n";
  cout <<" Reference : "<< left <<setw(26) <<reference <<"\n";
  cout <<"\n";
  cout <<" Amount Paid : KES " << setw(16) << fixed <<setprecision(2) <<amount <<"\n";
  cout <<" Balance Before : KES " << setw(16) <<p.balanceBefore <<"\n";
  cout <<" Balance After : KES " << setw(16) <<p.balanceAfter <<"\n";
  cout <<"\n";

  if (targetBill->paid) {
    cout <<" Bill #" << billId << " is now FULLY PAID! \n";
  }else{
    double stillOwed = targetBill->totalAmount - targetBill->amountPaid;
    cout << " Still owed on biil: KES "<< setw(13) << stillOwed <<"\n";
  }
  if (c->balance < 0){
    cout <<" Credit o n account: KES "<< setw(11)<<abs(c->balance) <<"\n";
  }
  cout <<"\n"; 
}
void viewPaymentHistory(){
  cout <<"\n Payment History \n";
  if (customers.empty()) {
    cout <<" No customers yet.\n"; return;
  }
  int id;
  cout <<" Enter Customer ID: ";
  cin >> id;
  Customer* c = findCustomerById(id);
  if (!c){cout << "Not found.\n"; return;}
  cout <<"\n Customer : "<< c->name <<"\n";
  cout <<" Meter No. : "<< c->meterNumber <<"\n";
  cout <<" Balance : KES "<< fixed << setprecision(2) <<c->balance <<"\n";
  
  if (c->payments.empty()){
    cout <<" No payment made yet.\n"; return;}
  cout <<"\n" << left
       << setw(6) << "Pay #"
       << setw(8) <<"Bill #"
       << setw(14) <<"Date"
       << setw(16) <<"Method"
       << setw(18) <<"Reference"
       << setw(12) <<"Amount(KES)"
       << setw(14) <<"Bal After(KES)"
       <<"\n";
  cout <<" " <<string(88, '-') << "\n";
  
  double totalPaid = 0;
  for (const auto& p : c->payments){
    cout <<" " << left
         << setw(6) << p.paymentId
         << setw(8) <<p.billId
         << setw(14) <<p.date
         << setw(16) <<p.method
         << setw(18) <<p.reference
         << setw(12) << fixed << setprecision(2) << p.amountPaid
         << setw(14) <<p.balanceAfter
         <<"\n";
    totalPaid += p.amountPaid;     
  }

  cout <<" " << string(88, '-') <<"\n";
  cout <<" Total Paid to Date: KES" << fixed << setprecision(2) << totalPaid <<"\n\n";

}

void generateBill(){
  cout <<"\n Generate Bill \n";

  if (customers.empty()){
    cout <<"No customers registered yet.\n";
    return;
  }
  int id;
  cout << "Enter Customer ID: ";
  cin >>id;

  Customer* c = findCustomerById(id);
  if (c == nullptr) {
    cout <<"Customer not found. \n";
    return;
  }
  //find unbilled records
  double totalUnits = 0.0;
  int unbilledCount = 0;
  
  for (const auto& r : c->records) {
    if(!r.billed){
      totalUnits += r.unitsUsed;
      unbilledCount++;
    }
  }

  if (unbilledCount == 0) {
    cout <<"No unbilled usage records found for "<< c->name <<".\n";
    return;
  }

  //get bill dates from users
  string issueDate, dueDate;
  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  cout <<"Issue Date (YYYY-MM-DD): ";
  getline(cin, issueDate);
  cout <<"Due Date (YYYY-MM-DD): ";
  getline(cin, dueDate);

  Bill bill;
  bill.billId = c->bills.size() + 1;
  bill.issueDate = issueDate;
  bill.dueDate = dueDate;
  bill.totalUnits = totalUnits;
  bill.paid = false;

  calculateTieredCost(totalUnits, bill);

  for (auto& r : c->records) {
    if (!r.billed){
      r.billed = true;
    }
  }
  c->bills.push_back(bill);
  c->balance += bill.totalAmount;

  //print the bill receipts
  cout <<"\n";
  cout <<" WATER BILL RECEIPT \n";
  cout <<"Customer :" << left << setw(27) <<c->name <<"\n";
  cout <<"Meter No :" << left << setw(27) <<c->meterNumber <<"\n";
  cout <<"Bill No :" << left << setw(27) <<bill.billId <<"\n";
  cout <<"Issued :" << left << setw(27) <<issueDate <<"\n";
  cout <<"Due Date :" << left << setw(27) <<dueDate <<"\n";
  cout <<"\n";
  cout <<" USAGE BREAKDOWN\n";
  cout << fixed << setprecision(2);
  cout << "Total Units : "<< setw(6) <<totalUnits<<"m³" << setw(18)<<"\n";
  cout <<"\n";
  cout <<" TIER CHARGES \n";
  if (bill.tier1Units > 0)
  cout <<"Tier 1("<<setw(5) <<bill.tier1Units <<"m³ * KES 50) = "<< setw(8) <<bill.tier1Cost <<"\n";
  if (bill.tier2Units > 0)
  cout <<"Tier 2("<<setw(5) <<bill.tier2Units <<"m³ * KES 75) = "<< setw(8) <<bill.tier2Cost <<"\n";
  if (bill.tier3Units > 0)
  cout <<"Tier 3("<<setw(5) <<bill.tier3Units <<"m³ * KES 100) = "<< setw(8) <<bill.tier3Cost <<"\n";
  if (bill.tier4Units > 0)
  cout <<"Tier 4("<<setw(5) <<bill.tier4Units <<"m³ * KES 150) = "<< setw(8) <<bill.tier4Cost <<"\n";

  cout <<"Service Charge =" <<setw(8) <<bill.serviceCharge <<"\n";
  cout <<"\n";
  cout <<"TOTAL DUE : KES "<< setw(21) <<bill.totalAmount <<"\n";
  cout <<"\n";
}
// view bills
void viewBills(){
  cout <<"View Bills\n";
  if (customers.empty()){
    cout <<" No Customers registered yet. \n";
    return;
  }

  int id;
  cout << " Enter Customer ID: ";
  cin >> id;

  Customer* c = findCustomerById(id);
  if (c == nullptr){
    cout << "Customer not found.\n";
    return;
  }
  cout <<"\n Customer : " << c->name <<"\n";
  cout << "Meter No. : " << c->meterNumber <<"\n";
  cout << "Balance : KES" << fixed << setprecision(2) <<c->balance <<"\n";

  if (c->bills.empty()){
    cout <<" No bills generated yet.\n";
    return;
  }
  cout << "\n"
       << left
       << setw(8) <<"Bill #"
       << setw(14) <<"Issued"
       << setw(14) <<"Due Date"
       << setw(12) <<"Units (m³)"
       << setw(14) <<"Amount (KES)"
       << setw(10) <<"Status"
       << "\n";
  cout <<" " << string(72, '-') <<"\n";
  
  for (const auto& b : c->bills) {
    cout <<" "
         << left
         << setw(8) <<b.billId
         << setw(14) <<b.issueDate
         << setw(14) <<b.dueDate
         << setw(12) <<b.totalUnits
         << setw(14) << fixed << setprecision(2)<<b.totalAmount
         << setw(10) <<(b.paid? "PAID" : "UNPAID")
         << "\n";
  }
  cout <<"\n";
}

void registerCustomer() {
  cout <<" Register New Customer \n";
  Customer c;

  c.id = customers.size() + 1;
  c.meterNumber = generateMeterNumber(c.id);
  c.balance = 0.0;
  c.lastReading = 0.0;

  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  
  cout << " Full Name :";
  getline(cin, c.name);

  cout <<" Phone Number :";
  getline(cin, c.phone);

  cout <<" Opening Meter Reading (m³): ";
  cin >> c.lastReading;

  customers.push_back(c);

  cout <<"\n Customer registered successfully!\n";
  cout <<" Meter Number assigned: "<<c.meterNumber <<"\n";
  cout <<" Customer ID: "<<c.id<<"\n";

}

void recordUsage() {
  cout <<"Record Water Usage \n";
  if (customers.empty()){
    cout <<" No customers registered yet. Please register first.\n";
    return;
  }
  int id;
  cout <<"Enter Customer ID: ";
  cin >>id;

  Customer* c = findCustomerById(id);
  if (c == nullptr) {
    cout <<"Customer ID "<< id <<"not found.\n";
    return;
  }

  cout << " Customer : "<< c->name <<"\n";
  cout << "Meter Number : "<< c->meterNumber <<"\n";
  cout << "Last Reading : "<< c->lastReading <<"\n";

  double currentReading;
  cout <<"Current Reading (m³): ";
  cin >> currentReading;

  if (currentReading < c->lastReading) {
    cout <<"Error: Current reading ("<< currentReading << ") cannot be less than last reading ("<< c->lastReading <<").\n";
    return;
  }
  
  string date;
  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  cout <<" Date (YYYY-MM-DD) : ";
  getline(cin, date);

  WaterRecord wr;
  wr.recordId = c->records.size() + 1;
  wr.date = date;
  wr.previousReading = c->lastReading;
  wr.currentReading = currentReading;
  wr.unitsUsed = currentReading - c->lastReading;
  wr.billed = false;

  c->records.push_back(wr);
  c->lastReading = currentReading;

  cout <<"Usage recorded! \n";
  cout << " Units Used this period : " << wr.unitsUsed <<"m³\n";

}
void viewUsageHistory(){
  cout <<"View Usage History\n";
  if (customers.empty()){
    cout <<" No Customers registered yet. \n";
    return;
  }

  int id;
  cout << " Enter Customer ID: ";
  cin >> id;

  Customer* c = findCustomerById(id);
  if (c == nullptr){
    cout << "Customer not found.\n";
    return;
  }
  cout <<"\n Customer : " << c->name <<"\n";
  cout << "Meter No. : " << c->meterNumber <<"\n";

  if (c->records.empty()){
    cout <<" No usage records yet.\n";
    return;
  }
  cout << "\n"
       << left
       << setw(5) <<"#"
       << setw(14) <<"Date"
       << setw(14) <<"Prev (m³)"
       << setw(14) <<"Current (m³)"
       << setw(12) <<"Used (m³)"
       << setw(10) <<"Billed?"
       << "\n";
  cout <<" " << string(70, '-') <<"\n";
  
  for (const auto& r : c->records) {
    cout <<" "
         << left
         << setw(5) <<r.recordId
         << setw(14) <<r.date
         << setw(14) <<r.previousReading
         << setw(14) <<r.currentReading
         << setw(12) <<r.unitsUsed
         << setw(10) <<(r.billed ? "Yes" : "No")
         << "\n";
  }
  cout <<"\n";
}

void listCustomers() {
  cout <<"\n Registered Customers\n";

  if (customers.empty()) {
    cout <<" No customers registered yet.\n";
    return;
  }

  cout <<"\n"
       << left
       << setw(5) <<"ID"
       << setw(20) <<"Name"
       << setw(12) <<"Meter No."
       << setw(15) <<"Phone"
       << setw(14) <<"Last Read (m³)"
       << setw(10) <<"Balance (KES)"
       <<"\n"; 
       
  cout << " " <<string(78, '-') << "\n";


  for (const auto& c : customers){
    cout <<" "
         << left
         << setw(5) <<c.id
         << setw(20) <<c.name 
         << setw(12) <<c.meterNumber  
         << setw(15) <<c.phone 
         << setw(14) <<c.lastReading
         << setw(10) <<c.balance 
         <<"\n";      
  } 
  cout <<"\n";     
}

void displayMenu(){
  cout <<"\n";
  cout <<"SMART WATER METER & PAYMENT SYSTEM \n";
  cout <<"\n";
  cout <<" 1. Register New Customer\n";
  cout <<" 2. List All Customers\n";
  cout <<" 3. Record Water Usage\n";
  cout <<" 4. View Usage History\n";
  cout <<" 5. Generate Bill\n";
  cout <<" 6. View Bill\n";
  cout <<" 7. Make Payment\n";
  cout <<" 8. View Payment History\n";
  cout <<" 0. Exit\n";
  cout <<"\n";
  cout <<" Enter your choice: ";
}
int main(){
  int choice;
  do{
    displayMenu();
    cin >> choice;

    switch(choice){
      case 1: registerCustomer(); break;
      case 2: listCustomers(); break;
      case 3: recordUsage(); break;
      case 4: viewUsageHistory(); break; 
      case 5: generateBill(); break;   
      case 6: viewBills(); break;   
      case 7: makePayment(); break; 
      case 8: viewPaymentHistory(); break;  
      case 0:cout <<"\n Thankyou for visiting! Existing system\n"; break;
      default:cout <<"\n Invalid Choice. Please select from Menu\n"; break;               
    }
  } while (choice != 0);
  return 0;
}

