/*
 * Parses & prints informations about classfile.
 */

#include <iostream>
#include <fstream>
#include <cassert>
#include <chrono>
#include <string_view>

#include <ClassFile/Error.hpp>

#include <ClassFile/ClassFile.hpp>
#include <ClassFile/Parser.hpp>
#include <ClassFile/Serializer.hpp>
#include <ClassFile/Util.hpp>

static bool PrintDetails{false};

void PrintConstInfo(size_t i, const ClassFile::ConstantPool& cp)
{
  std::cout << "ConstPool[" << i << "] = " << cp[i]->GetName();

  auto errOrString = cp.LookupString(i);
  auto errOrDesc   = cp.LookupDescriptor(i);

  if(!errOrString.IsError())
    std::cout << " " << errOrString.Get();

  if(!errOrDesc.IsError())
    std::cout << " " << errOrDesc.Get();

  std::cout << '\n';
}

std::string_view GetOperandTypeName(ClassFile::Instruction::OperandType type)
{
  switch(type)
  {
    using Type = ClassFile::Instruction::OperandType;
    case Type::TypeU8:  return "U8";
    case Type::TypeU16: return "U16";
    case Type::TypeS8:  return "S8";
    case Type::TypeS16: return "S16";
    case Type::TypeS32: return "S32";
  }

  return "UNKNOWN_TYPE";
}

void PrintInstrInfo(const ClassFile::Instruction& instr)
{
  std::cout << instr.GetMnemonic() << " (0x";
  std::cout << std::hex << (int)instr.Op << std::dec << ")";

  if(instr.GetNOperands() > 0)
    std::cout << ": Opreands[";
  else
    std::cout << '\n';

  for(size_t i = 0; i < instr.GetNOperands(); i++)
  {
    std::cout << GetOperandTypeName(instr.GetOperandType(i));
    std::cout << "{";
    std::cout <<  instr.GetOperand(i).Get();
    std::cout << "}";

    if(i + 1 != instr.GetNOperands())
      std::cout << ", ";
    else
      std::cout << "]\n";
  }
}

void PrintConstPool(const ClassFile::ConstantPool& cp)
{

  std::cout << "Const pool entries: " 
    << cp.GetSize()<< '\n';

  if(!PrintDetails)
    return;


  for(auto i{1}; i < cp.GetCount(); i++)
  {
    if(cp[i] == nullptr)
      continue;

    std::cout << "  ";

    PrintConstInfo(i, cp);

    if(i+1 == cp.GetCount())
      std::cout << "\n";
  }

}

void PrintFlags(const ClassFile::ClassFile& cf)
{
  std::cout << "Access flags: 0x" << std::uppercase << std::hex
    << cf.AccessFlags << std::nouppercase << std::dec;

  auto flagStrings = cf.FlagsToStrs();

  if(flagStrings.size() == 0)
  {
    std::cout << '\n';
    return;
  }

  std::cout << " (";

  for(auto i = 0u; i < flagStrings.size(); i++)
  {
    std::cout << flagStrings[i];

    if(i+1 != flagStrings.size())
      std::cout << ", ";
  }

  std::cout << ")\n";
}

void PrintMethods(const ClassFile::ClassFile& cf)
{
  std::cout << "Methods: " << cf.Methods.size();

  for(const auto& method : cf.Methods)
  {
    std::cout << "\n  ";
    std::cout << cf.ConstPool.LookupString(method.DescriptorIndex).Get();
    std::cout << " - ";
    std::cout << cf.ConstPool.LookupString(method.NameIndex).Get();
    std::cout << " [";

    auto flagStrings = method.FlagsToStrs();

    for(auto i = 0u; i < flagStrings.size(); i++)
    {
      std::cout << flagStrings[i];

      if(i+1 != flagStrings.size())
        std::cout << ", ";
    }

    std::cout << "]";

    if(!PrintDetails)
      continue;

    auto codeItr = std::find_if(method.Attributes.begin(), method.Attributes.end(), 
        [](const auto& pAttr) { return pAttr->GetType() == ClassFile::AttributeInfo::Type::Code;});


    const auto* pCodeAttr = 
      codeItr == method.Attributes.end() ? nullptr : 
      static_cast<const ClassFile::CodeAttribute*>((*codeItr).get());


    if(pCodeAttr)
    {
      std::cout << " :\n";

      for(const auto& instr : pCodeAttr->Code)
      {
        std::cout << "   ";
        PrintInstrInfo(instr);
      }

      //print exception table
      if(pCodeAttr->ExceptionTable.size() > 0)
        std::cout<< "  Exception table:\n";

      for(const auto& itr : pCodeAttr->ExceptionTable)
      {
        std::cout << "   " 
          << "StartPC{" << itr.StartPC << "}, "
          << "EndPC{" << itr.EndPC << "}, "
          << "HandlerPC{" << itr.HandlerPC<< "}, "
          << "CatchType{";

        if(itr.CatchType)
          std::cout << cf.ConstPool.LookupString(itr.CatchType).Get();
        else
          std::cout << itr.CatchType;
          
        std::cout << "}\n";
      }


      //check if a line number table attribute exists in the code attribute
      auto lineTableItr = std::find_if(pCodeAttr->Attributes.begin(), pCodeAttr->Attributes.end(), 
          [](const auto& pAttr) { return pAttr->GetType() == ClassFile::AttributeInfo::Type::LineNumberTable;});

      const auto* pLineTableAttr = 
        lineTableItr == pCodeAttr->Attributes.end() ? nullptr :
        static_cast<const ClassFile::LineNumberTableAttribute*>((*lineTableItr).get());

      if(pLineTableAttr)
      {
        std::cout << "  Line number table:\n";
        for(auto mapping : pLineTableAttr->LineNumberMap)
          std::cout << "    PC{" << mapping.PC << "}, Line{" << mapping.LineNumber << "}\n";
      }

    }

    std::cout << "\n";
  }

}

void PrintFields(const ClassFile::ClassFile& cf)
{
  std::cout << "\nFields: " << cf.Fields.size();

  for(const auto& field : cf.Fields)
  {
    std::cout << "\n  ";

    //Print flags
    for(auto flagView : field.FlagsToStrs())
    {
      std::string flag{flagView};
      std::transform(flag.begin(), flag.end(), flag.begin(), ::tolower);
      std::cout << flag << ' ';
    }

    //Print FieldType
    auto desc = cf.ConstPool.LookupString(field.DescriptorIndex).Get();
    auto fieldType = ClassFile::FieldDescriptorToTypeStr(desc).Get();

    std::cout << fieldType << ' ';

    //Print Name
    std::cout << cf.ConstPool.LookupString(field.NameIndex).Get() << ';';
  }

  std::cout << "\n";
}

void PrintClassInfo(const ClassFile::ClassFile& cf)
{
  std::cout << "Magic number: 0x" << std::uppercase << std::hex
    << cf.Magic << '\n' << std::nouppercase << std::dec;

  std::cout << "Classfile version: " 
    << cf.MajorVersion << '.' << cf.MinorVersion << '\n';

  PrintConstPool(cf.ConstPool);
  PrintFlags(cf);

  std::cout << "ThisClass: ";
  std::cout << cf.ConstPool.LookupString(cf.ThisClass).Get() << '\n';

  std::cout << "SuperClass: ";
  std::cout << cf.ConstPool.LookupString(cf.SuperClass).Get() << '\n';

  std::cout << "Interfaces count: " << cf.Interfaces.size();

  if(PrintDetails)
  {
    for(size_t i = 0; i < cf.Interfaces.size(); i++)
    {
      std::cout << "\n  Interfaces[" << i << "] = " <<cf.Interfaces[i];

      if(i+1 == cf.Interfaces.size())
        std::cout << '\n';
    }
  }

  std::cout << '\n';

  PrintMethods(cf);
  PrintFields(cf);

}

int main(int argc, char** argv)
{
  if(argc < 2)
  {
    std::cout << "Usage: " << argv[0] << " <classfile> (--details)\n";
    return -1;
  }

  for(auto i = 2; i < argc; i++)
  {
    using namespace std::literals;

    if("--details"sv == argv[i]) 
    {
      PrintDetails = true;
      continue;
    }

    std::cout << "Unknown flag / argument: \"" << argv[i] << "\"\n";
    return -2;
  }

  std::ifstream infile{argv[1], std::ios::ate};

  if(!infile.good())
  {
    std::cout << "Unable to open file \"" << argv[1] << "\"\n";
    return -3;
  }

  auto fileSize = infile.tellg();
  infile.seekg(0);


  auto before = std::chrono::high_resolution_clock::now();
  auto errOrClass = ClassFile::Parser::ParseClassFile(infile);
  auto after = std::chrono::high_resolution_clock::now();

  if(errOrClass.IsError())
  {
    std::cout << "ERROR: " << errOrClass.GetError().What << '\n';
    return -1;
  }

  std::cout << "Parsed " << infile.tellg() << '/' << fileSize << " bytes ";
  std::cout << "in ~" << std::chrono::duration_cast<std::chrono::nanoseconds>(after-before).count() / 1000000.0f << " milliseconds\n\n";

  ClassFile::ClassFile cf = errOrClass.Release();
  PrintClassInfo(cf);

  return 0;
}
