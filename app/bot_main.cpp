#include <iostream>

struct Test {
    std::string a{};
};

class Container {

    Container(std::string var1) : m_var1(std::move(var1)) {}

    Container(const Container&& other) noexcept : m_var1(std::move(other.m_var1)) {};

private:
    std::string m_var1{};

};



int main() {

    std::shared_ptr<Test> ptr = std::make_shared<Test>("cool");

    {
        std::shared_ptr<const Test> ptr1 = ptr;

        const std::shared_ptr<Test> ptr2 = ptr;

        const std::shared_ptr<const Test> ptr3 = ptr;

        ptr2->a = 10;

        std::cout << ptr.use_count() << std::endl;
    } // fin du scope

    std::cout << ptr.use_count() << std::endl;

    return 0;
}