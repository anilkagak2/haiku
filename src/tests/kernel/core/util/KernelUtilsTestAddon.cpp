#include <TestSuite.h>
#include <TestSuiteAddon.h>
//#include <AVLTreeMapTest.h>
#include <SinglyLinkedListTest.h>
#include <VectorTest.h>

BTestSuite* getTestSuite() {
	BTestSuite *suite = new BTestSuite("KernelUtils");
//	suite->addTest("AVLTreeMap", AVLTreeMapTest::Suite());
	suite->addTest("SinglyLinkedList", SinglyLinkedListTest::Suite());
	suite->addTest("Vector", VectorTest::Suite());
	return suite;
}
