#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
//#include <bits/stdc++.h>
#include <sstream>

// ECE474 Homework 3
// Due December 6, 2020 - Extra Credit Deadline
// Due December 9, 2020 - Absolute Deadline

using namespace std;
enum Operation
{
	REG = 'R',
	ADD = '+',
	SUB = '-',
	MUL = '*',
	GT = '>',
	LT = '<',
	EQ = '=',
	SHR = 'r',
	SHL = 'l'
};
unordered_map<string, vector<string>> inputs;
unordered_map<string, vector<string>> outputs;
unordered_map<string, vector<string>> wires;
unordered_map<string, vector<string>> registers;
vector<string> fileVariables;

struct node
{
	string resource;			 //Operation which needs to be done
	string output;				 //Variable being outputed by node
	vector<string> inputs;		 //Inputs required by node to start
	vector<node *> predecessors; //Nodes generating the inputs
	vector<node *> succesors;	 //Nodes waiting for the output
	int state = -1;				 //State(clock cycle) where it starts
	bool scheadule = false;		 //Node is completed - needed for LIST_R
	bool done = false;
};
unordered_map<int, node> nodes;
unordered_map<string, vector<int>> resources;
string getBitWidth(string variable);
string getNumber(string variable);
string getVariableNames();
string appendToInput(string input, string output);
vector<vector<string>> operations;
void writeVerilogFile(string verilogFile, vector<vector<string>> results);
bool isItSigned(string variable);
void createNode(vector<string> expression);
void connectNodes();
void setALAPTimes(int latency);
void setTimes(node *theNode, int latency);
void doLISTR(int latency);
bool allNodesDone();
string globaLatency;
int numBits;

int ModuleIndex = 0; //Global Variable for use in the function convertExpression
//This variables will used as the node counter
string decToBinary(int n)
{
	std::string r;
	while (n != 0)
	{
		r = (n % 2 == 0 ? "0" : "1") + r;
		n /= 2;
	}
	int numberBits = numBits + 1; // numBits is the max number when allocating the register size in verilog
	if (r.size() != numberBits)
	{
		int missing = numberBits - r.size();
		std::string zeroes(missing, '0');
		r = zeroes + r;
	}
	return r;
}

int countBits(int n)
{
	int count = 0;
	// While loop will run until we get n = 0
	while (n)
	{
		count++;
		// We are shifting n to right by 1
		// place as explained above
		n = n >> 1;
	}
	return count;
}

bool isValidOperator(string op)
{
	bool isValid = false;
	vector<string> ops{"+", "-", "*", ">>", "<<", "?", ">", "<", "==", "%", "/"};
	for (string x : ops)
	{
		if (op.compare(x) == 0)
		{
			isValid = true;
			break;
		}
	}
	return isValid;
}

bool validVariables(vector<string> expression)
{
	bool isValid = true;
	//cout << "in valid Variables: ";
	for (auto exp : expression)
	{
		int ctr = 0;
		//cout << "exp:" << exp << endl;
		for (auto x : fileVariables)
		{
			/*cout << x << " ";
			cout << "counter" << ctr << endl;*/
			if (exp.compare(x) == 0)
			{ // if variable (exp) is found in fileVariables (declared), break out of the inner for loop and check the next 'exp'
				break;
			}
			ctr++;
		}
		//This is producing an error for
		if (ctr == fileVariables.size())
		{ // variable (exp) is not found in fileVariables, thus, not valid
			//cout << "counter, fileVariable:" << ctr << " "<< fileVariables.size() << endl;
			cout << "ERROR! Variable '" << exp << "' is missing or not declared!" << endl;
			return false;
		}
	}

	return isValid;
}

// Converts math expressions in the format of
//  d = a + b
// to a string of the fo
//  ADD #(.WIDTH(<BitWidth>))line<ModuleIndex>(a,b,d);
// Supports the operations: REG, ADD, SUB, MUL, COMP, MUX2x1, SHR, SHL
string convertExpresion(vector<string> expression)
{
	string CLK = "CLK";
	string RST = "RST";
	string result = "";
	ModuleIndex = ModuleIndex + 1;
	string bitWidth = "";

	vector<string> vars;
	for (unsigned int i = 0; i < expression.size(); i += 2)
	{
		//cout << "expression" << expression[i] << endl;
		if (expression[i] != "")
			vars.push_back(expression[i]);
	}

	if (!validVariables(vars))
	{
		// cout << "ERROR: Missing variable!" << endl;
		exit(0);
	}

	// if not register expression
	if (expression.size() != 3 /* && validateVariables() == true*/)
	{
		string op = expression[3];
		//validVariables(expression);
		bitWidth = (getBitWidth(expression[0]) > getBitWidth(expression[4])) ? getBitWidth(expression[0]) : getBitWidth(expression[4]); //Bitwidth is determined by inputs
		string inputsToOp = appendToInput(expression[2], bitWidth) + "," + appendToInput(expression[4], bitWidth);
		//COMP; Comparison
		result += (isItSigned(expression[2]) || isItSigned(expression[4])) ? "S" : ""; //Inputs signed?
		if (op.compare(">") == 0)
			result += "COMP #(.WIDTH(" + (bitWidth) + "))line" + to_string(ModuleIndex) + "(" + inputsToOp + "," + expression[0] + "," + "1'b0" + "," + "1'b0" + ");";

		else if (op.compare("<") == 0)
			result += "COMP #(.WIDTH(" + (bitWidth) + "))line" + to_string(ModuleIndex) + "(" + inputsToOp + "," + "1'b0" + "," + expression[0] + "," + "1'b0" + ");";

		else if (op.compare("==") == 0)
			result += "COMP #(.WIDTH(" + (bitWidth) + "))line" + to_string(ModuleIndex) + "(" + inputsToOp + "," + "1'b0" + "," + "1'b0" + "," + expression[0] + ");";

		else if (isValidOperator(op))
		{
			bitWidth = getBitWidth(expression[0]); //Bitwidth is determined by output or wire
			inputsToOp = appendToInput(expression[2], bitWidth) + "," + appendToInput(expression[4], bitWidth);
			//ADD; Addition
			if (op.compare("+") == 0)
				result += "ADD ";

			//SUB; Subtraction
			else if (op.compare("-") == 0)
				result += "SUB ";

			//MUL; Multiplication
			else if (op.compare("*") == 0)
				result += "MUL ";

			//SHR; Shift Right
			else if (op.compare(">>") == 0)
				result += "SHR ";

			//SHL; Shift Left
			else if (op.compare("<<") == 0)
				result += "SHL ";

			//MUX2x1; Multiplex from 2 to 1. Special Case
			else if (op.compare("?") == 0)
			{
				result = (isItSigned(expression[4]) || isItSigned(expression[6])) ? "S" : "";
				return result + "MUX2x1 #(.WIDTH(" + (bitWidth) + "))line" + to_string(ModuleIndex) + "(" + appendToInput(expression[4], bitWidth) + "," + appendToInput(expression[6], bitWidth) + //inputs
					   "," + expression[2] + "," + expression[0] + ");";
			}

			result += "#(.WIDTH(" + (bitWidth) + "))line" + to_string(ModuleIndex) + "(" + inputsToOp + "," + expression[0] + ");";
		}
		else
		{
			cout << "Error: Invalid operator" << endl;
			exit(0);
		}

		return result;
	}
	//REG
	else
	{
		bitWidth = getBitWidth(expression[0]);
		return "REG #(.WIDTH(" + (bitWidth) + "))line" + to_string(ModuleIndex) + "(" + CLK + "," + RST + "," + appendToInput(expression[2], bitWidth) + "," + expression[0] + ");";
	}

	return "UNEXPECTED EXPRESSION or ERROR\n";
}

/*
Creates a string which will append/concaneted a input according
to the output variable which can be signed or unsigned.
Verilog sign extension:
	extended[15:0] <= {{8{a[7]}}, a[7:0]};
Padd zero:
	extended[15:0] <= {8'b0, a[7:0]};
*/
string appendToInput(string input, string bitWidth)
{
	string result = "", mostSigBit = "";
	int difference = stoi(bitWidth) - stoi(getBitWidth(input));
	if (difference == 0)
		result = input;
	else if (difference > 0)
	{ //Need to add bits
		mostSigBit = to_string(stoi(getBitWidth(input)) - 1);
		if (isItSigned(input))
		{ //sign extend
			if (mostSigBit != "0")
				result = "{{" + to_string(difference) + "{" + input + "[" + mostSigBit + "]}}," +
						 input + "[" + mostSigBit + ":0]}";
			else //Sign extending a bit of size 1
				result = "{" + to_string(difference) + "'b0," +
						 input + "}";
		}
		else
		{ //padd zeros
			result = "{" + to_string(difference) + "'b0," +
					 input + "[" + mostSigBit + ":0]}";
		}
	}
	else
	{ //Need to remove bits
		mostSigBit = to_string(stoi(bitWidth) - 1);
		result = input + "[" + mostSigBit + ":0]";
	}
	return result;
}

/**
Given a variable name, seaches through all global variable vectors
to find the bit width of the parameter variable.
All variables names are "SHOULD" be uniquely named, so we can search through all
vectors until we find it.
*/
string getBitWidth(string variable)
{
	string bitWidth = "0";
	if (outputs.count(variable) > 0)
		bitWidth = outputs[variable][1];
	else if (wires.count(variable) > 0)
		bitWidth = wires[variable][2];
	else if (registers.count(variable) > 0)
		bitWidth = registers[variable][2];
	else if (inputs.count(variable) > 0)
		bitWidth = inputs[variable][1];
	return bitWidth;
}

/*
Check if the output variable of an operations is signed
or unsigned. CHECK if output of component determines if
component is unsgined or signed.
*/
bool isItSigned(string variable)
{
	bool signedV = false;
	if (outputs.count(variable) > 0)
		signedV = (outputs[variable][0] == "s");
	else if (wires.count(variable) > 0)
		signedV = (wires[variable][1] == "s");
	else if (registers.count(variable) > 0)
		signedV = (registers[variable][0] == "s");
	else if (inputs.count(variable) > 0)
		signedV = (inputs[variable][0] == "s");
	return signedV;
}

/**
Gets numbers from a string. (e.g. Int8 -> 8; Int64 -> 64;
Int128 -> 128; H3LL0 -> 30)
*/
string getNumber(string x)
{
	string result = "";
	for (unsigned int i = 0; i < x.size(); i++)
		if (isdigit(x[i]))
			result += x[i];
	return result;
}

string convertDeclaration(vector<string> input)
{
	string declaration = "";
	string bits;
	int x;
	if (input.at(0) == "input")
	{
		declaration += "input ";
	}
	else if (input.at(0) == "output")
	{
		//declaration += "output ";
		declaration += "output reg ";
	}
	else if (input.at(0) == "register")
	{
		declaration += "reg ";
	}
	else
	{
		//declaration += "wire ";
		declaration += "reg ";
	}
	if (input.at(1) != "Int1")
	{
		bits = getNumber(input.at(1));
		stringstream degree(bits);
		degree >> x;
		x--;
		bits = to_string(x);
		string temp = "[" + bits + ":0] ";
		declaration += temp;
	}

	for (unsigned int i = 2; i < input.size(); i++)
	{
		declaration += input.at(i);
		if (i != input.size() - 1)
		{
			declaration += " ";
		}
	}
	declaration += ";";
	return declaration;
}

////prints the state of the nodes
/* void printNodes()
{
	for (const auto &[key, value] : nodes)
	{
		cout << "Node" << key << " "
			 << "Inputs:"; //" : " << value.inputs << endl;
		for (int i = 0; i < value.inputs.size(); i++)
		{
			cout << value.inputs.at(i) << " ";
		}
		cout << "Time " << value.state << endl;
	}
	for (const auto &[key, value] : resources)
	{
		cout << "Resource: " << key << " ";
		cout << value.size() << endl;
	}
	return;
}
 */
// inputs["a":<sign, bits>, "b":<sign, bit>]

int readFile(string input_filename, int latency, string output_filename = "verilogFile")
{
	string tempString = "";
	string line;
	ifstream myfile;
	vector<vector<string>> results;
	myfile.open(input_filename);

	//while(getline(myfile, line)){
	while (getline(myfile, line, '\n'))
	{
		cout << "line:" << line << "\n";
		vector<string> lineSplit;
		string actuaLine = line;
		string token;
		string delimiter = " ";
		size_t pos = 0;

		//The following two lines of code remove white space
		// characters at the end of the line
		std::size_t endWhite = line.find_last_not_of(" \t\f\v\n\r");
		if (endWhite != std::string::npos)
		{
			line.erase(endWhite + 1);
		};

		// FIGURE OUT HOW TO REMOVE COMMENTS

		do
		{
			pos = line.find(delimiter);
			if (line.substr(0, pos).find("//") != std::string::npos)
				break;
			token = line.substr(0, pos);
			lineSplit.push_back(token);
			line.erase(0, pos + delimiter.length());
		} while ((pos) != string::npos);
		if (lineSplit[0] == "input" || lineSplit[0] == "output" || lineSplit[0] == "variable" || lineSplit[0] == "register")
		{
			if (lineSplit[0] == "input")
			{
				for (unsigned int i = 2; i < lineSplit.size(); i++)
				{
					vector<string> temp;
					char first = lineSplit[1][0];
					string sign = "s";
					if (first == 'U')
					{
						sign = "u";
					}
					temp.push_back(sign);
					string bitWidth = getNumber(lineSplit[1].substr(lineSplit[1].size() - 2));
					temp.push_back(bitWidth);
					string key = lineSplit[i];
					if (lineSplit[i][lineSplit[i].size() - 1] == ',')
					{

						key = lineSplit[i].substr(0, lineSplit[i].size() - 1);
					}
					inputs[key] = temp;
					fileVariables.push_back(key);
				}
			}
			else if (lineSplit[0] == "output")
			{
				for (unsigned int i = 2; i < lineSplit.size(); i++)
				{
					vector<string> temp;
					char first = lineSplit[1][0];
					string sign = "s";
					if (first == 'U')
					{
						sign = "u";
					}
					temp.push_back(sign);
					string bitWidth = getNumber(lineSplit[1].substr(lineSplit[1].size() - 2));
					temp.push_back(bitWidth);
					string key = lineSplit[i];
					if (lineSplit[i][lineSplit[i].size() - 1] == ',')
					{
						key = lineSplit[i].substr(0, lineSplit[i].size() - 1);
					}
					outputs[key] = temp;
					fileVariables.push_back(key);
				}
			}
			else if (lineSplit[0] == "variable")
			{ //variable replaced wires?
				for (unsigned int i = 2; i < lineSplit.size(); i++)
				{
					vector<string> temp;
					char first = lineSplit[1][0];
					string sign = "s";
					if (first == 'U')
					{
						sign = "u";
					}
					temp.push_back(lineSplit[i]);
					temp.push_back(sign);
					string bitWidth = getNumber(lineSplit[1].substr(lineSplit[1].size() - 2));
					temp.push_back(bitWidth);
					string key = lineSplit[i];
					if (lineSplit[i][lineSplit[i].size() - 1] == ',')
					{
						key = lineSplit[i].substr(0, lineSplit[i].size() - 1);
					}
					wires[key] = temp;
					fileVariables.push_back(key);
				}
			}
			else if (lineSplit[0] == "register")
			{
				for (unsigned int i = 2; i < lineSplit.size(); i++)
				{
					vector<string> temp;
					char first = lineSplit[1][0];
					string sign = "s";
					if (first == 'U')
					{
						sign = "u";
					}
					temp.push_back(lineSplit[i]);
					temp.push_back(sign);
					string bitWidth = getNumber(lineSplit[1].substr(lineSplit[1].size() - 2));
					temp.push_back(bitWidth);
					string key = lineSplit[i];
					if (lineSplit[i][lineSplit[i].size() - 1] == ',')
					{
						key = lineSplit[i].substr(0, lineSplit[i].size() - 1);
					}
					registers[key] = temp;
					fileVariables.push_back(key);
				}
			}
			tempString += "\t" + convertDeclaration(lineSplit) + "\n";
			vector<string> v{tempString};
			results.push_back(v);
		}
		else if (lineSplit[0] != "")
		{ //Some lines are empty in the netlist
			createNode(lineSplit);
			//tempString += "\t" + convertExpresion(lineSplit) + "\n";
			tempString += "\t" + actuaLine + ";\n";
			vector<string> v{tempString, to_string(ModuleIndex - 1)};
			operations.push_back(v);
			//results.push_back(tempString);//Need to determine LIST_r
		}
		//cout << tempString;
		tempString = "";
	}
	connectNodes();
	setALAPTimes(latency);
	doLISTR(latency);
	//printNodes();
	////////////////////////////////Do List_R stuff here////////////////////////////////
	////////////////////////////////Do List_R stuff here////////////////////////////////
	writeVerilogFile(output_filename, results);
	return 0;
}
string getTransitionBlockCode()
{
	string code = "";
	code = "\talways @(posedge Clk, posedge Rst)\n";
	code += "\tbegin\n";
	code += "\t\tif(Rst == 1'b1)\n";
	code += "\t\t\tpresent_state = Wait;\n";
	code += "\t\telse\n";
	code += "\t\t\tpresent_state = next_state;\n";
	code += "\tend\n";
	return code;
}

string getOutputBlockCode()
{
	string code = "";
	code = "\talways @(posedge Clk)\n";
	code += "\tbegin\n";
	code += "\t\tif(present_state == Final)\n";
	code += "\t\t\tDone = 1'b1;\n";
	code += "\t\telse\n";
	code += "\t\t\tDone = 1'b0;\n";
	code += "\tend\n";
	return code;
}

string getStartingStateCode()
{
	string code = "";
	code = "\talways @(present_state)\n";
	code += "\tbegin\n";
	code += "\t\tcase(present_state)\n";
	return code;
}

string getEndingStateCode()
{
	string code = "";
	code += "\t\tendcase\n";
	code += "\tend\n";
	return code;
}
string getStateCaseCode(int state)
{
	string code = "";
	if (state == 0)
	{
		code += "\t\tWait:\n";
		code += "\t\tbegin\n";
		code += "\t\t\tif(Start == 1'b1)\n";
		code += "\t\t\t\tnext_state = sig1;\n";
		code += "\t\t\telse\n";
		code += "\t\t\t\tnext_state = Wait;\n";
	}
	else if (state == (stoi(globaLatency) + 2) - 1)
	{
		code += "\t\tFinal:\n";
		code += "\t\tbegin\n";
		code += "\t\t\t\tnext_state = Wait;\n";
	}
	else
	{
		code += "\t\tsig" + to_string(state) + ":\n";
		code += "\t\tbegin\n";
		for (int i = 0; i < nodes.size(); i++)
		{
			if (nodes[i].state == state)
			{
				code += "\t\t\t" + operations[i][0];
			}
		}
		if (state == stoi(globaLatency))
			code += "\t\t\t\tnext_state = Final;\n";
		else
		{
			code += "\t\t\t\tnext_state = sig" + to_string(state + 1);
			code += ";\n";
		}
	}
	code += "\t\tend\n";
	return code;
}

string getStatesCode()
{
	string code = "";
	for (unsigned int i = 0; i < (stoi(globaLatency) + 2); i++)
	{
		code += getStateCaseCode(i) + "\n";
	}
	return code;
}
/**
Creates the Verilog file given the results from the convertExpression
convertDeclaration functions.
*/
void writeVerilogFile(string verilogFile, vector<vector<string>> results)
{
	ofstream file;
	numBits = countBits(stoi(globaLatency) + 1) - 1;
	file.open(verilogFile + ".v");
	file << "`timescale 1ns / 1ps" << endl;
	file << "module "
		 << "HLSM "
		 << "(Clk, Rst, Start, Done, " << getVariableNames() << ");" << endl;
	file << "\tinput Clk, Rst, Start;" << endl;
	file << "\toutput reg Done;" << endl;
	file << "\treg[" << numBits << ":0] present_state, next_state;" << endl;
	for (unsigned int i = 0; i < results.size(); i++)
	{
		if (results[i].size() == 1)
			file << results[i][0];
	}
	for (unsigned int i = 0; i < (stoi(globaLatency) + 2); i++)
	{
		if (i == 0)
			file << "\tlocalparam Wait"
				 << " = " << numBits + 1 << "'b" << decToBinary(i) << ";" << endl;
		else if (i == (stoi(globaLatency) + 2) - 1)
			file << "\tlocalparam Final"
				 << " = " << numBits + 1 << "'b" << decToBinary(i) << ";" << endl;
		else
			file << "\tlocalparam sig" << i << " = " << numBits + 1 << "'b" << decToBinary(i) << ";" << endl;
	}
	for (unsigned int i = 0; i < results.size(); i++)
	{
		if (results[i].size() != 1)
			file << results[i][0] << results[i][1] << "\n";
	}
	file << getTransitionBlockCode() << endl;
	file << getOutputBlockCode() << endl;
	file << getStartingStateCode();
	file << getStatesCode();
	file << getEndingStateCode();
	file << "endmodule" << endl;
	return;
}

/**
Gets all variables names required to create the module intance in the Verilog file
from the inputs and outputs.
*/
string getVariableNames()
{
	string variables = "";
	for (const auto &myPair : inputs)
	{
		variables += myPair.first;
		variables += ", ";
	}
	for (const auto &myPair : outputs)
	{
		variables += myPair.first;
		variables += ", ";
	}
	variables = variables.substr(0, variables.size() - 2);
	return variables;
}

/**
Creates a node. The output should only be one variable. The amount of inputs
depends on the operation being done. For example...
	z = g ? d : e
This line has three inputs(g, d, & e).
However, if all operations need to have inputs of two. THEN, the MUX operation
needs to be splitted into two separed nodes each, making things more complicated
*/
void createNode(vector<string> expression)
{
	node newNode;
	vector<string> vars;
	for (unsigned int i = 0; i < expression.size(); i += 2)
	{
		if (expression[i] != "")
			vars.push_back(expression[i]);
	}

	if (!validVariables(vars))
		exit(0);

	newNode.output = expression[0];
	newNode.inputs.push_back(expression[2]);
	// if not register expression
	if (expression.size() != 3)
	{
		string op = expression[3];
		if ((op == ">") || (op == "<") || (op == "=="))
			newNode.resource = "LOGIC";
		newNode.inputs.push_back(expression[4]);
		if (isValidOperator(op))
		{
			if ((op == "+") || (op == "-"))
				newNode.resource = "ADD/SUB";
			else if ((op == "/") || (op == "%"))
				newNode.resource = "DIV/MOD";
			else if ((op == "*"))
				newNode.resource = "MULT";
			//MUX2x1; Multiplex from 2 to 1. Special Case
			else if ((op == ">>") || (op == "<<") || (op == "?"))
				newNode.resource = "LOGIC";
			if (op.compare("?") == 0)
				newNode.inputs.push_back(expression[6]); //third input
		}
		else
		{
			cout << "Error: Invalid operator" << endl;
			cin.get();
			exit(0);
		}
		nodes[ModuleIndex] = newNode;
		ModuleIndex += 1;
		return;
	}
	//REG
	else
	{
		newNode.resource = "REG";
		nodes[ModuleIndex] = newNode;
		ModuleIndex += 1;
		return;
	}
	cout << "UNEXPECTED EXPRESSION or ERROR\n";
}

/**
Connect every single node by populating the succesors and predeccesors vectors
of the nodes.
*/
void connectNodes()
{
	unsigned int i, j;
	for (i = 0; i < nodes.size(); ++i)
	{
		for (j = 0; j < nodes.size(); ++j)
		{
			if (i == j)
				continue;
			for (string input : nodes[i].inputs)
			{
				if (input == nodes[j].output)
				{
					nodes[i].predecessors.push_back(&nodes[j]);
					nodes[j].succesors.push_back(&nodes[i]);
				}
			}
		}
	}
	return;
}

/**
Assign a value for the state property for each node corresponding to ALAP time.
The nodes should have predeccesors and succesors populated in order for this
function to work. PROBABLY A RECUSIVE SOLUTION WOULD BE BETTER
I am defining branches as the group of nodes which are not connected to other
group of nodes. To identify branches, I will check the nodes at the end of the
DAG(nodes with no succesors).
*/
void setALAPTimes(int latency)
{
	for (unsigned int i = 0; i < nodes.size(); ++i)
	{
		if (nodes[i].succesors.size() == 0)
			setTimes(&nodes[i], latency);
	}
	return;
}

/**
Recursive helper? function to set the time property from the nodes sent by
setALAPTimes function.
*/
void setTimes(node *theNode, int latency)
{
	if (latency <= 0)
	{
		cout << "Error: Latency is not big enough to fit all nodes.\n\n\n";
		cin.get();
		exit(1);
	}
	if ((*theNode).resource == "MULT") //multiplier take two cycles
		--latency;
	if ((*theNode).resource == "DIV/MOD") //multiplier take two cycles
		latency -= 2;
	if ((*theNode).state > latency || (*theNode).state == -1)
		(*theNode).state = latency;
	for (int i = (*theNode).predecessors.size() - 1; i >= 0; i--)
		setTimes((*theNode).predecessors[i], latency - 1);

	return;
}

/**
Change all state values corresponding to LIST_R scheaduling.
Pseudo code can be found on lecture 17_HLS_Scheduling_2.pdf
*/
void doLISTR(int latency)
{
	//operations maps to vector of boolean which stands for the resource being used
	//unordered_map<string, vector<int>> resources;
	vector<tuple<int *, node *>> busyResources;
	vector<node *> nodesAtState;
	int currtime, slack;
	bool notFailed;
	vector<string> ops{"ADD/SUB", "MULT", "LOGIC", "DIV/MOD", "REG"};
	node *currNode; //CAREFUL. You can create a new node if the index is wrong
	for (string op : ops)
	{
		resources[op].reserve(1000);
		resources[op].push_back({0});
	}
	currtime = 1;
	do
	{
	CHECKVECTORAGAIN:
		for (unsigned int i = 0; i < busyResources.size(); ++i)
		{ //Check if any resource is busy
			--(*(get<0>(busyResources[i])));
			if (*(get<0>(busyResources[i])) == 0)
			{
				get<1>(busyResources[i])->done = true;
				busyResources.erase(busyResources.begin() + i, busyResources.begin() + i + 1);
				goto CHECKVECTORAGAIN; //Vectors are hard to deal with, so redo entire for loop
			}
		}

		for (unsigned int i = 0; i < nodes.size(); ++i)
		{ //Get all nodes at time L
			notFailed = true;
			for (node *theNode : nodes[i].predecessors)
			{
				if (!theNode->done)
				{
					notFailed = false;
					break;
				}
			}
			if (notFailed && !nodes[i].scheadule)
				nodesAtState.push_back(&nodes[i]);
		}

		for (unsigned int i = 0; i < nodesAtState.size(); ++i)
		{
			currNode = nodesAtState[i];
			slack = currNode->state - currtime;
			if (slack < 0)
			{
				cout << "ERROR: LATENCY IS NOT ENOUGHT TO FIT ALL NODES";
				cin.get();
				exit(0);
			}
			for (unsigned int j = 0; j < resources[currNode->resource].size(); ++j)
			{
				if ((resources[currNode->resource][j] == 0) && (!currNode->scheadule))
				{
					currNode->scheadule = true;
					currNode->state = currtime;
					resources[currNode->resource][j] = slack;
					if (currNode->resource == "DIV/MOD")
						resources[currNode->resource][j] = 3;
					else if (currNode->resource == "MULT")
						resources[currNode->resource][j] = 2;
					else
						resources[currNode->resource][j] = 1;
					busyResources.push_back({&resources[currNode->resource][j], currNode});
					break;
				}
			}
			if ((slack == 0) && (!currNode->scheadule))
			{												//if 0 slack and all resources are busy
				resources[currNode->resource].push_back(0); //Create new resource
				int index = resources[currNode->resource].size() - 1;
				if (currNode->resource == "DIV/MOD")
					resources[currNode->resource][index] = 3;
				else if (currNode->resource == "MULT")
					resources[currNode->resource][index] = 2;
				else
					resources[currNode->resource][index] = 1;
				busyResources.push_back({&resources[currNode->resource][index], currNode});
				currNode->scheadule = true;
			}
		}
		currtime += 1;
		nodesAtState.clear();
	} while (!allNodesDone()); //keep working until all nodes scheaduled
	return;
}

bool allNodesDone()
{
	for (unsigned int i = 0; i < nodes.size(); ++i)
	{
		if (!nodes[i].done)
			return false;
	}
	return true;
}

int main(int argc, const char *argv[])
{
	ifstream myfile;
	/*tuple<int, int> x;
	x = { 1,5 };
	int y = get<0>(x);*/
	globaLatency = argv[2];
	myfile.open(argv[1]);
	if (myfile)
	{
		if (argc == 4)
			readFile(argv[1], stoi(argv[2]), argv[3]);
		/* else
			readFile(argv[1]); */
	}
	else
	{
		//cout << "file doesn't exist" << "\n";
		return 0;
	}
	cout << "Press a key to continue" << endl;
	cin.get();
	return 0;
}
