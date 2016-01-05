#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <utility>
#include <gmpxx.h>



//Gauss-Jordan elimination

using namespace std;

void gcd(pair<mpz_class, mpz_class>& input)
{
	if (input.first == 0) {
		input.second = 1;
		return;
	}
	mpz_class first = input.first;
	mpz_class second = input.second;
	if (first > second) {
		first = second;
		second = input.first;
	}
	while (second != 0) {
		mpz_class temp = second;
		second = first%second;
		first = temp;
	}
	input.first /= first;
	input.second /= first;
	if (input.second < 0) {
		input.first *= -1;
		input.second *= -1;
	}
}

int main()
{
	string path("./variables.csv");
	ifstream inputFile(path);

	vector<mpz_class> answers;
	//answer line
	string rawAnswer;
	getline(inputFile, rawAnswer);
	istringstream stringy(rawAnswer);
	string number;
	while(getline(stringy, number, ',')) {
		answers.push_back(atol(number.c_str()));
	}

	//now read in the system
	vector<vector<pair<mpz_class, mpz_class> > > lines;
	while(getline(inputFile, rawAnswer)) {
		istringstream stringy(rawAnswer);
		vector<pair<mpz_class, mpz_class> > innerLine;
		while (getline(stringy, number, ',')) {
			pair<mpz_class, mpz_class>
				addPair(atol(number.c_str()), 1);
			innerLine.push_back(addPair);
		}
		lines.push_back(innerLine);
	}
	bool nonZero = false;
	for (int i = 0; i < lines.size(); i++) {
		pair<mpz_class, mpz_class> pivot(lines[i][i].second, lines[i][i].first);
		if (lines[i][i].first != 0) {
			lines[i][i].first = 1;
			lines[i][i].second = 1;
		} else {
			nonZero = true;
			continue;
		}
		for (int j = 0; j <= lines.size(); j++) {
			if (lines[i][j].first == 0 || i == j) {
				continue;
			}
			lines[i][j].first *= pivot.first;
			lines[i][j].second *= pivot.second;
			gcd(lines[i][j]);
		}
		for (int j = i + 1; j < lines.size(); j++) {
			pair<mpz_class, mpz_class> multiple(lines[j][i].first, lines[j][i].second);	
			lines[j][i] = pair<mpz_class, mpz_class>(0, 1);
			for (int k = i + 1; k <= lines.size(); k++) {
				pair<mpz_class, mpz_class> factor(multiple.first * lines[i][k].first, multiple.second * lines[i][k].second);
				gcd(factor);
				lines[j][k] = pair<mpz_class, mpz_class>(lines[j][k].first * factor.second - factor.first * lines[j][k].second, lines[j][k].second * factor.second);
				gcd(lines[j][k]);
			}
			if (nonZero) {
				for (int k = 0; k < i; k++) {
					if (lines[i][k].first == 0) {
						continue;
					}
					pair<mpz_class, mpz_class> factor(multiple.first * lines[i][k].first, multiple.second * lines[i][k].second);
					gcd(factor);
					lines[j][k] = pair<mpz_class, mpz_class>(lines[j][k].first * factor.second - factor.first * lines[j][k].second, lines[j][k].second * factor.second);
					gcd(lines[j][k]);
				}
			}
	
		}
	}
	// now eliminate upper half
	if (nonZero == false) {
		for (int i = lines.size() - 1; i > 0; i--) {
			if (lines[i][i].first == 0) {
				continue;
			}	
			pair<mpz_class, mpz_class> answer = lines[i][lines.size()];
			for (int j = i - 1; j >= 0; j--) {
				pair<mpz_class, mpz_class> multiple = lines[j][i];
				if (multiple.first == 0) {
					continue;
				}
				lines[j][i] = pair<mpz_class, mpz_class>(0, 1);
				lines[j][lines.size()].first = lines[j][lines.size()].first * multiple.second * answer.second - answer.first * multiple.first * lines[j][lines.size()].second;
				lines[j][lines.size()].second = lines[j][lines.size()].second * answer.second * multiple.second;
				gcd(lines[j][lines.size()]);
			}
		}	
	} else { 
		//have lower elements that are not zero
		for (int i = lines.size() - 1; i > 0; i--) {
			if (lines[i][i].first == 0) {
				continue;
			}	
			pair<mpz_class, mpz_class> answer = lines[i][lines.size()];
			for (int j = i - 1; j >= 0; j--) {
				//wrong multiple below!
				pair<mpz_class, mpz_class> multiple(lines[j][i].first * lines[i][i].second, lines[j][i].second * lines[i][i].first);
				if (multiple.first == 0) {
					continue;
				}
				gcd(multiple);
				lines[j][i] = pair<mpz_class, mpz_class>(0, 1);
				lines[j][lines.size()].first = lines[j][lines.size()].first * multiple.second * answer.second - answer.first * multiple.first * lines[j][lines.size()].second;
				lines[j][lines.size()].second = lines[j][lines.size()].second * answer.second * multiple.second;
				gcd(lines[j][lines.size()]);
				for (int k = 0; k < lines.size(); k++) {
					if (lines[i][k].first == 0 || k == i) {
						continue;
					}
					lines[j][k].first = lines[j][k].first * multiple.second * lines[i][k].second - lines[i][k].first * multiple.first * lines[j][k].second;
					lines[j][k].second = lines[j][k].second * lines[i][k].second * multiple.second;
					gcd(lines[j][k]);
				}
				for (int k = i + 1; k < lines.size(); k++) {
						if (lines[i][k].first == 0) {
							continue;
					}
					lines[j][k].first = lines[j][k].first * multiple.second * lines[i][k].second - lines[i][k].first * multiple.first * lines[j][k].second;
					lines[j][k].second = lines[j][k].second * lines[i][k].second * multiple.second;
					gcd(lines[j][k]);
				}

			}

		}
	}
	cout << "DIAGONAL FORM" << endl;
	for (int i = 0; i < lines.size(); i++) {
		for (int j = 0; j < lines.size(); j++) {
			if (lines[i][j].first == 0) {
				cout << "0 , ";
			} else {
				if (lines[i][j].second == 1) {
					cout << lines[i][j].first << " , ";
				}
				else {
					cout << lines[i][j].first << "/" << lines[i][j].second << " , ";
				}
			}
		}
		cout << " == " << lines[i][lines.size()].first << " / " << lines[i][lines.size()].second << endl;
	}

}
		
