/*
	$Id: PropertyConstructionTest1.h,v 1.4 2002/08/15 04:42:06 jrand Exp $
	
	This file defines a class for performing one test of BPropertyInfo
	functionality.
	
	*/


#ifndef PropertyConstructionTest1_H
#define PropertyConstructionTest1_H


#include "../common.h"
#include <PropertyInfo.h>

	
class PropertyConstructionTest1 :
	public TestCase {
	
private:
	void CheckProperty(BPropertyInfo *propTest,
	                   const property_info *prop_list,
	                   const value_info *value_list,
	                   int32 prop_count,
	                   int32 value_count,
	                   ssize_t flat_size,
	                   const char *flat_data);
	void CompareProperties(const property_info *prop1,
	                       const property_info *prop2,
	                       int prop_count);
	void CompareValues(const value_info *value1,
	                   const value_info *value2,
	                   int value_count);
	property_info *DuplicateProperties(const property_info *prop1, int prop_count);
	value_info *DuplicateValues(const value_info *value1, int value_count);
	
public:
	static Test *suite(void);
	void setUp(void);
	void PerformTest(void);
	PropertyConstructionTest1(std::string name = "");
	virtual ~PropertyConstructionTest1();
	};
	
#endif






