#pragma once

#include "Error.hpp"

namespace ClassFile
{

//FieldDescriptor Grammar:
//  FieldType
//
//FieldType grammar:
//  ObjectType
//  ArrayType
//  BasicType
//
ErrorOr<std::string> FieldDescriptorToTypeStr(std::string_view desc)
{
  //ObjectType grammar (spaces not included): 
  //L classname ;
  if(*desc.begin() == 'L' && *(desc.end()-1) == ';')
  {
    std::string objectName{desc.substr(1, desc.size()-2)};
    std::replace(objectName.begin(), objectName.end(), '/', '.');
    return objectName;
  }

  //ArrayType grammar (spaces not included): 
  //[ FieldDescriptor
  if(*desc.begin() == '[')
  {
    auto errOrType = FieldDescriptorToTypeStr(std::string_view{desc.data()+1, desc.size()-1});

    if(errOrType.IsError())
      return errOrType.GetError();

    std::string type = errOrType.Get();
    type += "[]";
    return type;
  }

  //BasicType: char corresponds to a integral type in the below list
  if(desc.size() == 1)
  {
    static std::tuple<char, std::string_view> basicTypes[] = 
    {
      {'B', "byte"},
      {'C', "char"},
      {'D', "double"},
      {'F', "float"},
      {'I', "int"},
      {'J', "long"},
      {'S', "short"},
      {'Z', "boolean"},
    };

    //probably faster than a map lookup
    for(auto basicType : basicTypes)
    {
      if(*desc.begin() == std::get<0>(basicType))
        return std::string{std::get<1>(basicType)};
    }
  }

  return Error{"invalid field descriptor."};
}


} //namespace ClassFile
