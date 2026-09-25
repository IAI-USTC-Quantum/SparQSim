#pragma once
#include <iostream>
#include <cmath> // for M_PI
#define PI 3.1415926

struct GateBase {
    int id;
    size_t digit;
    GateBase(std::string reg_, size_t digit_) : id(reg_.size()), digit(digit_) {};
    GateBase(int id_, size_t digit_) : id(id_), digit(digit_) {};
    // Virtual function with a base-class implementation
    virtual void display() const {
        std::cout << "Base Gate display function." << std::endl;
    }
};

struct Phase_Int : GateBase {
    double lambda;

    Phase_Int(std::string reg_, size_t digit_, double lambda_) :GateBase(reg_, digit_), lambda(lambda_) {
        // Logic to initialize other member variables
    }

    Phase_Int(int id_, size_t digit_, double lambda_) : GateBase(id_, digit_), lambda(lambda_) {
        // Logic to initialize other member variables
    }

    // Override the display function
    void display() const override;
};

struct Zgate_Int : Phase_Int {
    Zgate_Int(std::string reg_, size_t digit_) : Phase_Int(reg_, digit_, PI) {}

    Zgate_Int(int id_, size_t digit_) : Phase_Int(id_, digit_, PI) {}

    // Override Phase_Int's display function
    void display() const override;
};