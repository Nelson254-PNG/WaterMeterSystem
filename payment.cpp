#include "payment.h"
#include "global.h"
#include "customer.h"
#include "constants.h"
#include <iostream>
#include <iomanip>
using namespace std;

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

  if (amount <= 0){
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
// function of a validmpesacode

bool isValidMpesaCode(const string& code){
  if(code.length() != 10){ return false;}
  
  for (char ch : code){
    if(!isupper(ch) && !isdigit(ch)){return false;}
  }
  return true;
}
// verify if the code has been used
bool isCodeAlreadyUsed(const string& code){
  for (const auto& c : customers){
    for (const auto& p : c.payments){
      if (p.method == "M-pesa" && p.reference == code){ return true;}
    }
  }
  return false;
}
// pay by mpesa paybill number
void payByMpesaPaybill(){
  cout << "\n  Pay via M-pesa Paybill \n";
  if (customers.empty()){cout << "No Customers Yet.\n"; return;}
  
  int id;
  cout <<" Customer ID: ";
  cin >>id;

  Customer* c = findCustomerById(id);
  if (!c) {cout << "Not found.\n"; return;}
  
  cout <<"\n "<< c->name << " Balance: KES "
       << fixed << setprecision(2) << c->balance <<"\n\n Unpaid Bills:\n";
  cout << " " << left <<setw(8) << "Bill #" << setw(14) << "Issued"
       <<setw(14) << "Total(KES)" << setw(14) << "PaiD(KES)" << "Remaining\n";
  cout << " " << string(68, '-') << "\n";
  
  bool hasUnpaid = false;
  for (const auto& b : c->bills){
    if (!b.paid) {
      cout << " " << left << setw(8) << b.billId << setw(14) << b.issueDate
           << setw(14) << fixed << setprecision(2) << b.totalAmount
           << setw(14) << b.amountPaid
           << (b.totalAmount - b.amountPaid) <<"\n";
      hasUnpaid = true;     
    }
  }
  if (!hasUnpaid){ cout <<"No unpaid bills!\n"; return;}

  int billId;
  cout << "\n Enter Bill # to pay:";
  cin >>billId;

  Bill* tb = nullptr;
  for (auto& b : c->bills)
      if (b.billId == billId && !b.paid){tb = &b; break;}
  if(!tb) {cout <<" Bill not Found or already paid.\n"; return;} 
  double remaining = tb->totalAmount - tb->amountPaid;

  cout <<"\n";
  cout <<" PAY VIA M-PESA - FOLLOW THESE STEPS \n";
  cout <<"\n";
  cout <<" 1. Go to M-Pesa menu on your phone \n";
  cout <<" 2. Select Lipa Na M-Pesa > Pay Bill \n";
  cout <<" Business No. :" << MPESA_PAYBILL_NUMBER <<"\n";
  cout <<" Account No. : "<<c->meterNumber <<"-B" <<billId <<"\n";
  cout <<" Amount : KES" << fixed << setprecision(2) << remaining <<"\n";
  cout <<"\n";
  cout <<"\n Once you receive the M-Pesa confirmations SMS,\n";
  cout <<" enter the transation code below.\n";

  string code;
  bool validCode = false;

  while (!validCode){cout <<"\n M-Pesa Transaction Code (or 0 to cancel):"; 
    cin >> code;

    if (code == "0"){cout << "Payment cancelled.\n"; return;}

    for (auto& ch : code) ch = toupper(ch);
    if(!isValidMpesaCode(code)){
      cout <<" Invalid format. Must be exactly 10 lettere/digits.\n";
      continue;
    }
    if(isCodeAlreadyUsed(code)){
      cout <<" This transaction code has already been recorded. Check and re-enter.\n";
      continue;
    }
    validCode = true;
  }
  // this confirms the amount the customer is sending
  double amount;
  cout <<" Confirm Amount Paid (KES): ";
  cin >> amount;

  if (amount <= 0){cout<<" Must be greator than zero.\n"; return;}
  string payDate;
  cin.ignore(numeric_limits<streamsize>::max(), '\n');
  cout << "Date (YYYY-MM-DD): ";
  getline(cin, payDate);
  
  Payment p;
  p.paymentId = c->payments.size() + 1;
  p.billId = billId;
  p.date = payDate;
  p.method = "M-Pesa";
  p.reference = code;
  p.amountPaid = amount;
  p.balanceBefore = c->balance;

  tb->amountPaid += amount;
  if(tb->amountPaid >= tb->totalAmount - 0.01) tb->paid = true;
  c->balance -= amount;

  p.balanceAfter = c->balance;
  c->payments.push_back(p);

  cout <<"\n M-Pesa payment recorded - code: "<< code <<"\n";
  cout << "Amount :KES"<< fixed << setprecision(2)<< amount <<"\n";
  cout <<" New Balance : KES" << c->balance;
  if (c->balance < 0) cout << "Credit on account.";
  cout <<"\n";
  if(tb->paid) cout <<"Bill #" <<billId << "is Fully PAID\n";


}