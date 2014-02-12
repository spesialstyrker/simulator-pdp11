#include <iostream>
#include "cpu.h"

#define src 3
#define dst 1
#define clr 0
#define set 1
#define Zbit 04
#define Nbit 010
#define Cbit 01
#define Vbit 02
#define Negbit 040

CPU::CPU(Memory *memory)/*{{{*/
{
  this->debugLevel = Verbosity::off;
  this->instructionCount = 0;
  this->memory = memory;
}
/*}}}*/

CPU::~CPU()/*{{{*/
{
  delete this->memory;
}
/*}}}*/

short CPU::EA(short encodedAddress)/*{{{*/
{
  char mode = (encodedAddress & 070) >> 3;
  char reg = encodedAddress & 07;
  short decodedAddress = 0;
  std::string modeType = "Not Set!";

  switch(mode)
  {
    case 0: // General Register
      {
        decodedAddress = encodedAddress;
        modeType = "General Register";
        break;
      }

    case 1: // Deferred Register
      {
        decodedAddress = this->memory->Read(encodedAddress);
        modeType = "Deferred Register";
        break;
      }

    case 2: // Autoincrement
      {
        // Check for immediate PC addressing
        if (reg == 07)
        {
          // Point to the word after the instruction word
          decodedAddress = this->memory->Read(PC) + 02;
          this->memory->Write(PC, decodedAddress);
          modeType = "Immediate PC";
        }

        else
        {
          /*
           * Retrieve the memory address from encodedAddress
           * and then increment the pointer stored in encodedAddress
           */
          decodedAddress = this->memory->Read(encodedAddress);
          this->memory->Write(encodedAddress, decodedAddress + 02);
          modeType = "Autoincrement";
        }

        break;
      }

    case 3: // Autoincrement Deferred
      {
        // Check for absolute PC addressing
        if (reg == 07)
        {
          // Retrieve the operand from the address given
          decodedAddress = this->memory->Read(encodedAddress);
          modeType = "Absolute PC";
        }

        else
        {
          // Do we increment the pointer or address?
          short pointer = memory->Read(encodedAddress);
          short decodedAddress = memory->Read(pointer);
          memory->Write(encodedAddress, decodedAddress + 02);
          modeType = "Autoincrement Deferred";
        }

        break;
      }

    case 4: // Autodecrement
      {
        decodedAddress = this->memory->Read(encodedAddress);
        this->memory->Write(encodedAddress, decodedAddress - 02);
        modeType = "Autodecrement Deferred";
        break;
      }

    case 5: // Autodecrement Deferred
      {
        // Do we decrement the pointer or address?
        short pointer = memory->Read(encodedAddress);
        short decodedAddress = memory->Read(pointer);
        memory->Write(encodedAddress, decodedAddress - 02);
        modeType = "Autodecrement Deferred";
        break;
      }

    case 6: // Indexed
      {
        // Check for relative PC addressing
        if (reg == 07)
        {
          // Calculate the distance between address of operand and the PC
          decodedAddress = encodedAddress - this->memory->Read(PC);
          modeType = "Relative PC";
        }

        else
        {
          /*
           * The address is the some of the contents of the operand plus the specified
           * offset which is the word following the instruction
           */
          decodedAddress = this->memory->Read(PC + 02) + this->memory->Read(encodedAddress);
          modeType = "Indexed";
        }

        break;
      }

    case 7: // Deferred Indexed
      {
        // Check for deferred relative PC addressing
        if (reg == 07)
        {
          /*
           * Calculate the distance between value at address pointed
           * to by operand and the PC
           */
          short value = this->memory->Read(encodedAddress);
          decodedAddress = value - this->memory->Read(PC);
          modeType = "Deferred Relative PC";
        }

        else
        {
          /*
           * The address is the some of the contents pointed to by the
           * operand plus the specified offset which is the word following
           * the instruction
           */
          short value = this->memory->Read(encodedAddress);
          decodedAddress = this->memory->Read(PC + 02) + this->memory->Read(value);
          modeType = "Deferred Indexed";
        }

        break;
      }

    default:
      break;
  }

  if (this->debugLevel == Verbosity::verbose)
  {
    std::cout << "Mode Type: " << modeType << "(" << static_cast<int>(mode) << ")" << std::endl;
    std::cout << "Register: " << static_cast<int>(reg) << std::endl;
  }

  return decodedAddress;
}
/*}}}*/

/*
 * Takes in the program counter register value in order to
 * be able to fetch, decode, and execute the next instruction
 * .
 */ 
int CPU::FDE()/*{{{*/
{
  short instruction;          // Instruction word buffer
  short instructionBits[6];   // Dissected instruction word
  short tmp;                  // Used as a temporary buffer for instruction operations
  short src_temp;             // Stores the working value of the src register
  short dst_temp;             // Stores the working value of the dst register

  // Decoder lambda function declarations/*{{{*/
  auto address = [&] (const int i) { return (instructionBits[i] << 3) + instructionBits[i - 1]; };
  auto update_flags = [&] (const int i, const int bit) 
  {
    if (i == 0) { 
      short temp = memory->Read(PS);
      temp = temp & ~(bit);
      memory->Write(PS, temp);
    }
    else {
      short temp = memory->Read(PS);
      temp = temp | bit;
      memory->Write(PS, temp);
    }
  };
  auto resultIsZero = [&] (const int result) { result == 0? update_flags(1,Zbit) : update_flags(0,Zbit); };  // Update Zbit
  auto resultLTZero = [&] (const int result) { result < 0? update_flags(1,Nbit) : update_flags(0,Nbit); };  // Update Nbit
  /*}}}*/

  // Instruction fetch/*{{{*/

  // Fetch the instruction and increment PC
  instruction = this->memory->ReadInstruction();
  this->memory->IncrementPC();
  ++this->instructionCount;

  // Optional instruction fetch state dump/*{{{*/
  if (debugLevel == Verbosity::verbose)
  {
    std::cout << "********************************************************************************" << std::endl;
    std::cout << "                                        BREAK" << std::endl;
    std::cout << "********************************************************************************" << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "********************************************************************************" << std::endl;
    std::cout << std::endl;
    std::cout << "                              INSTRUCTION #" << std::dec << instructionCount << std::endl;
    std::cout << std::endl;
    std::cout << "********************************************************************************" << std::endl;
    std::cout << "Fetched instruction: " << std::oct << instruction << std::endl;
    std::cout << std::endl;
    std::cout << "Dumping current register contents..." << std::endl;
    std::cout << "R0: " << std::oct << this->memory->Read(R0) << std::endl;
    std::cout << "R1: " << std::oct << this->memory->Read(R1) << std::endl;
    std::cout << "R2: " << std::oct << this->memory->Read(R2) << std::endl;
    std::cout << "R3: " << std::oct << this->memory->Read(R3) << std::endl;
    std::cout << "R4: " << std::oct << this->memory->Read(R4) << std::endl;
    std::cout << "R5: " << std::oct << this->memory->Read(R5) << std::endl;
    std::cout << "SP: " << std::oct << this->memory->Read(SP) << std::endl;
    std::cout << "PC: " << std::oct << this->memory->Read(PC) << std::endl;
    std::cout << std::endl;
    std::cout << "Processor status word: " << std::endl;
    std::cout << "N: " << std::oct << (this->memory->Read(PS) & 0x8) << std::endl;
    std::cout << "Z: " << std::oct << (this->memory->Read(PS) & 0x4) << std::endl;
    std::cout << "V: " << std::oct << (this->memory->Read(PS) & 0x2) << std::endl;
    std::cout << "C: " << std::oct << (this->memory->Read(PS) & 0x1) << std::endl;
  }
  /*}}}*/
  /*}}}*/

  // Decode & execute/*{{{*/
  instructionBits[0] = (instruction & 0000007);
  instructionBits[1] = (instruction & 0000070) >> 3; 
  instructionBits[2] = (instruction & 0000700) >> 6;
  instructionBits[3] = (instruction & 0007000) >> 9;
  instructionBits[4] = (instruction & 0070000) >> 12;
  instructionBits[5] = (instruction & 0700000) >> 15; // This is actually only giving us the final bit of the 16-bit word

  if(instructionBits[4] == 0) /*{{{*/
  {
    switch(instructionBits[3])
    {
      case 0: 
        {
          switch(instructionBits[2])
          {
            case 0: /*{{{*/
              {
                switch(instructionBits[5])
                {
                  case 0: /*{{{*/
                    {
                      switch(instructionBits[0])
                      {
                        case 0:
                          {
                            return 0; // HALT
                          }

                        case 1:
                          {
                            return 1; // WAIT
                          }

                        case 5:
                          {
                            return 5; // RESET
                          }

                        default:
                          break;
                      }

                      /*
                       * FIX THIS
                       */
                      //case 1:
                      //{ // BPL loc (loc is an offset dependent on ABS or REL addressing
                      //dst_temp = EA(address(dst));   // Get effective address for destination
                      //if (memory->Read(PS) & Nbit)  // N = 0? loc --> (PC)
                      //{
                      //// Needs some work ***********************
                      //if(memory->Read(PC) & Negbit) 
                      //{
                      //dst_temp = dst_temp + memory->Read();
                      //memory->Write(PC, dst_temp * 2);
                      //}
                      //}

                      //return instruction;
                      //default:
                      //break;
                      //}

                      case 1:/*{{{*/
                      { // JMP dst
                        dst_temp = EA(address(dst)); // Get effective address
                        memory->Write(PC, dst_temp); // effective address --> (PC) unconditional
                        break;
                      }
                      /*}}}*/

                      case 2: /*{{{*/
                      {
                        switch(instructionBits[1])
                        {
                          case 0:
                            { // RTS reg
                              tmp = (01 << 3) | instructionBits[0];
                              memory->Write(PC, EA(tmp));                 // reg --> (PC)
                              memory->Write(EA(tmp),memory->StackPop());  // pop reg
                              return instruction;
                            }

                          case 4: 
                            {
                              switch(instructionBits[0])
                              {
                                case 1:
                                  { // CLC - Clear C
                                    update_flags (0, Cbit);
                                    return instruction;
                                  }

                                case 2:
                                  { // CLV - Clear V
                                    update_flags (0, Vbit);
                                    return instruction;
                                  }

                                case 4:
                                  { // CLZ - Clear Z
                                    update_flags (0, Zbit);
                                    return instruction;
                                  }

                                default:
                                  break;
                              }
                            }

                          case 5:
                            {  // CLN - Clear N
                              update_flags (0, Nbit);
                              return instruction;
                            }

                          case 6: 
                            {
                              switch(instructionBits[0])
                              {
                                case 1:
                                  { // SEC - Set C
                                    update_flags (1, Cbit);
                                    return instruction;
                                  }
                                case 2:
                                  { // SEV - Set V
                                    update_flags (1, Vbit);
                                    return instruction;
                                  }
                                case 4:
                                  { // SEZ - Set Z
                                    update_flags (1, Zbit);
                                    return instruction;
                                  }

                                default:
                                  break;
                              }
                            }
                          case 7:
                            { // SEN - Set N
                              update_flags (1, Nbit);
                              return instruction;
                            }

                          default:
                            break;
                        }
                      }
                      /*}}}*/

                      case 3:/*{{{*/
                      { // SWAB dst - swap bytes of word at dst
                        dst_temp = EA(address(dst));             // Get effective address
                        tmp = memory->Read(dst_temp);            // Get value at effective address
                        short byte_temp = tmp << 8;             // Create temp and give it LSByte of value in MSByte
                        tmp = (tmp >> 8) & 0000777;             // Shift MSByte into LSByte and clear MSByte
                        tmp = byte_temp + tmp;                  // Finalize the swap byte
                        memory->Write(dst_temp, byte_temp);      // Write to register
                        return instruction;
                      }
                      /*}}}*/

                      case 4: /*{{{*/
                      {
                        switch(instructionBits[5])
                        {
                          case 0:
                            { // BR loc - unconditional branch
                              tmp = address(dst);           // Get offset
                              // Need to figure out the offset stuff
                              return instruction;
                            }
                          case 1:
                            { // BMI loc - Branch on minus
                              tmp = address(dst);           // Get offset
                              return instruction;
                            }
                          default:
                            break;
                        }
                      }
                      /*}}}*/
                    }
                    /*}}}*/
                }
              }
              /*}}}*/

            case 1: /*{{{*/
              {
                switch(instructionBits[2])
                {
                  case 0: /*{{{*/
                    {
                      switch(instructionBits[5])
                      {
                        case 0:
                          { // BNE loc - Branch if Not Equal (zero)
                            tmp = address(dst);
                            return instruction;
                          }
                        case 1:
                          { // BHI loc - Branch if Higher (neither Z nor C set)
                            tmp = address(dst);
                            return instruction;  // BHI loc
                          }
                        default:
                          break;
                      }
                    }
                    /*}}}*/

                  case 4: /*{{{*/
                    {
                      switch(instructionBits[5])
                      {
                        case 0:
                          { // BEQ loc - Brach if EQual (zero)
                            tmp = address(dst);
                            return instruction;
                          }
                        case 1:
                          { // BLOS loc - Branch if Lower Or Same (either C or Z set)
                            tmp = address(dst);
                            return instruction;
                          }
                        default:
                          break;
                      }
                    }
                    /*}}}*/

                  default:
                    break;
                }
              }
              /*}}}*/

            case 2: /*{{{*/
              {
                switch(instructionBits[2])
                {
                  case 0:/*{{{*/
                    {
                      switch(instructionBits[5])
                      {
                        case 0:
                          { // BGE loc - Branch if Greater or Equal (zero)
                            tmp = address(dst);
                            return instruction;
                          }
                        case 1:
                          { // BVC loc - Branch if oVerflow Clear
                            tmp = address(dst);
                            return instruction;
                          }
                        default:
                          break;
                      }
                    }
                    /*}}}*/

                  case 4: /*{{{*/
                    {
                      switch(instructionBits[5])
                      {
                        case 0:
                          { // BLT loc - Branch if Less Than (zero)
                            tmp = address(dst);
                            return instruction;
                          }
                        case 1:
                          { // BVS loc - Branch if oVerflow Set
                            tmp = address(dst);
                            return instruction;
                          }
                        default:
                          break;
                      }
                    }
                    /*}}}*/

                  default:
                    break;
                }
              }
              /*}}}*/

            case 3: /*{{{*/
              {
                switch(instructionBits[2])
                {
                  case 0: /*{{{*/
                    {
                      switch(instructionBits[5])
                      {
                        case 0:
                          { // BGT loc - Branch if Greater Than
                            tmp = address(dst);
                            return instruction;
                          }
                        case 1:
                          { // BCC loc - Branch if Carry Clear (C = 0)
                            tmp = address(dst);
                            return instruction;
                          }
                        default:
                          break;
                      }
                    }
                    /*}}}*/

                  case 4: /*{{{*/
                    {
                      switch(instructionBits[5])
                      {
                        /*
                         * FIX THIS
                         */

                        //case 0:
                        //{ // BLE loc - Branch if Lower or Equal (zero)
                        //dst_temp = EA(address(dst));      // Get effective address
                        //offset = memory->Read(dst_temp);  // Get offset 
                        //offset = offset << 2;             // Mult offset by 2
                        //tmp = memory->Read(PS);           // Get current process status
                        //(((tmp & 0x04) >> 2) & (((tmp & 0x08) >> 3) ^ ((tmp & 0x02) >> 1))) == 1? memory->Write(PC,offset) : ;     // Write offset to PC if Z(N^V)
                        //return instruction;
                        //}
                        case 1:
                          { // BCS loc - Branch if Carry Set (C = 1)
                            tmp = address(dst);
                            return instruction;
                          }
                        default:
                          break;
                      }
                    }
                    /*}}}*/

                  default:
                    break;
                }
              }
              /*}}}*/

            case 4:/*{{{*/
              { // JSR reg, dst
                // Need to figure this out
                return instruction;
              }
              /*}}}*/

            case 5: /*{{{*/
              {
                switch(instructionBits[5])
                {
                  case 0: /*{{{*/
                    {
                      switch(instructionBits[2])
                      {
                        case 0:
                          { // CLR dst - Clear Destination
                            dst_temp = EA(address(dst));  // Get effective address
                            memory->Write(dst_temp, 0);   // Clear value at address
                            update_flags(1,Zbit);         // Set Z bit - No change to other condition codes
                            return instruction;
                          }
                        case 1:
                          { // COM dst - Compliment Destination
                            dst_temp = EA(address(dst));  // Get effective address
                            tmp = memory->Read(dst_temp); // Get value at address
                            tmp = ~tmp;                   // Compliment value
                            memory->Write(dst_temp, tmp); // Write compiment to memory
                            resultIsZero(tmp);            // Update Z bit
                            (tmp & 0400000) == 1? update_flags(1,Nbit) : update_flags(0,Nbit); // Update N bit
                            update_flags(1,Cbit);         // Update C bit
                            update_flags(0,Vbit);         // Update V bit
                            return instruction;
                          }
                        case 2:
                          { // INC dst - Increment Destination
                            dst_temp = EA(address(dst));   // Get effective address
                            tmp = memory->Read(dst_temp);  // Get value at address
                            tmp == 0077777? update_flags(1,Vbit) : update_flags(0,Vbit);  // Update V bit
                            tmp++;                        // Increment value
                            memory->Write(dst_temp, tmp); // Write to memory
                            resultIsZero(tmp);            // Update Z bit
                            resultLTZero(tmp);            // Update N bit
                            return instruction;
                          }
                        case 3:
                          { // DEC dst - Decrement Destination
                            dst_temp = EA(address(dst));  // Get effective address
                            tmp = memory->Read(dst_temp); // Get value at address
                            tmp--;                        // Decrement value
                            memory->Write(dst_temp, tmp); // Write to memory
                            resultIsZero(tmp);            // Update Z bit
                            resultLTZero(tmp);            // Update N bit
                            update_flags(0,Cbit);         // Update C bit
                            update_flags(0,Vbit);         // Update V bit
                            return instruction;
                          }
                        case 4:
                          { // NEG dst - Gives 2's Compliment of destination
                            dst_temp = EA(address(dst));  // Get effective address
                            tmp = memory->Read(dst_temp); // Get value at address
                            tmp = ~tmp + 1;               // Get 2's comp of value
                            memory->Write(dst_temp,tmp);  // Write to memory
                            resultIsZero(tmp);            // Update Z bit
                            resultLTZero(tmp);            // Update N bit
                            tmp == 00? update_flags(0,Cbit) : update_flags(1,Cbit);       // Update C bit
                            tmp == 0100000? update_flags(1,Vbit) : update_flags(0,Vbit);  // Update V bit
                            return instruction;
                          }
                        case 5:
                          { // ADC - Add the carry to destination
                            dst_temp = EA(address(dst));  // Get effective address
                            tmp = memory->Read(dst_temp); // Get value at address
                            short tmpC = memory->Read(PS);// Get current value of PS
                            tmp = tmp + (tmpC & 0000001); // Add C bit to value
                            memory->Write(dst_temp,tmp);  // Write to memory
                            resultIsZero(tmp);            // Update Z bit
                            resultLTZero(tmp);            // Update N bit
                            tmp == 0200000? update_flags(1,Cbit) : update_flags(0,Cbit);  // Update C bit
                            tmp == 0100000? update_flags(1,Vbit) : update_flags(0,Vbit);  // Update V bit
                            return instruction;
                          }
                        case 6:
                          { // SBC Subtract the carry from destination
                            dst_temp = EA(address(dst));  // Get effective address
                            tmp = memory->Read(dst_temp); // Get value at address
                            short tmpC = memory->Read(PS);// Get current value of PS
                            tmp = tmp - (tmpC & 0000001); // Add C bit to value
                            memory->Write(dst_temp,tmp);  // Write to memory
                            resultIsZero(tmp);            // Update Z bit
                            resultLTZero(tmp);            // Update N bit
                            tmp == (0 & ((tmpC & 0000001) == 0))? update_flags(0,Cbit) : update_flags(1,Cbit);  // Update C bit
                            tmp == 0100000? update_flags(1,Vbit) : update_flags(0,Vbit);  // Update V bit
                            return instruction;  // SBC dst
                          }
                        case 7:
                          { // TST dst - Tests if dst is 0 (0 - dst)
                            dst_temp = EA(address(dst));  // Get effective address
                            tmp = memory->Read(dst_temp); // Get value at address
                            tmp = 0 - tmp;                // Perform test
                            resultIsZero(tmp);            // Update Z bit
                            resultLTZero(tmp);            // Update N bit
                            update_flags(0,Cbit);         // Update C bit
                            update_flags(0,Vbit);         // Update V bit   
                            return instruction;
                          }
                        default:
                          break;
                      }
                    }
                    /*}}}*/

                  case 1: /*{{{*/
                    {
                      switch(instructionBits[2])
                      {
                        /*
                         * FIX THIS
                         */
                        //case 0:
                        //{ // CLRB dst - Clear Byte
                        //memory->SetByteMode;
                        //dst_text = EA(address(dst));
                        //tmp = memory->Read(dst_temp);
                        //tmp = tmp & 0777000;
                        //memory->Write(dst_temp,tmp);
                        //return instruction;
                        //}
                        case 1:
                          { // COMB dst - Compliment byte at destination
                            dst_temp = EA(address(dst));  // Get effective address
                            tmp = memory->Read(dst_temp); // Get value at address
                            tmp = (tmp & 0xFF00) + ~(tmp | 0xFF00); // Compliment value
                            memory->Write(dst_temp, tmp); // Write compiment to memory
                            resultIsZero(tmp);            // Update Z bit
                            (tmp & 0400000) == 1? update_flags(1,Nbit) : update_flags(0,Nbit); // Update N bit
                            update_flags(1,Cbit);         // Update C bit
                            update_flags(0,Vbit);         // Update V bit
                            return instruction;
                          }
                        case 2:
                          { // INCB dst - Increment byte at destination
                            dst_temp = EA(address(dst));   // Get effective address
                            tmp = memory->Read(dst_temp);  // Get value at address
                            (tmp & 0x00FF) == 0x007F? update_flags(1,Vbit) : update_flags(0,Vbit);  // Update V bit
                            tmp++;                        // Increment value
                            memory->Write(dst_temp, tmp);  // Write to memory
                            resultIsZero(tmp);            // Update Z bit
                            resultLTZero(tmp);            // Update N bit
                            return  instruction;
                          }
                        case 3:
                          { // DECB dst
                            dst_temp = EA(address(dst));  // Get effective address
                            tmp = memory->Read(dst_temp); // Get value at address
                            tmp--;                        // Decrement value
                            memory->Write(dst_temp, tmp); // Write to memory
                            resultIsZero(tmp);            // Update Z bit
                            resultLTZero(tmp);            // Update N bit
                            update_flags(0,Cbit);         // Update C bit
                            update_flags(0,Vbit);         // Update V bit
                            return instruction;
                          }
                        case 4:
                          { // NEGB - Negate Byte
                            dst_temp = EA(address(dst));  // Get effective address
                            tmp = memory->Read(dst_temp); // Get value at address
                            tmp = (tmp & 0xFF00) + (~(tmp | 0xFF00) + 1);               // Get 2's comp of value
                            memory->Write(dst_temp,tmp);  // Write to memory
                            resultIsZero(tmp);            // Update Z bit
                            resultLTZero(tmp);            // Update N bit
                            (tmp & 0x00FF) == 0? update_flags(0,Cbit) : update_flags(1,Cbit);       // Update C bit
                            (tmp & 0x00FF) == 0x0080? update_flags(1,Vbit) : update_flags(0,Vbit);  // Update V bit
                            return instruction;
                          }

                        case 5:
                          return instruction;  // ADCB dst

                        case 6:
                          return instruction;  // SBCB dst

                        case 7:
                          return instruction;  // TSTB dst

                        default:
                          break;
                      }
                    }
                    /*}}}*/

                  default:
                    break;
                }
              }
              /*}}}*/

            case 6: /*{{{*/
              {
                switch(instructionBits[5])
                {
                  case 0: /*{{{*/
                    {
                      switch(instructionBits[2])
                      {
                        case 0:
                          return instruction;  // ROR dst

                        case 1:
                          return instruction;  // ROL dst

                        case 2:
                          return instruction;  // ASR dst

                        case 3:
                          return instruction;  // ASL dst

                        default:
                          break;
                      }
                    }
                    /*}}}*/

                  case 1: /*{{{*/
                    {
                      switch(instructionBits[2])
                      {
                        case 0:
                          return instruction;  // RORB dst
                        case 1:
                          return instruction;  // ROLB dst

                        case 2:
                          return instruction;  // ASRB dst

                        case 3:
                          return instruction;  // ASLB dst

                        default:
                          break;
                      }
                    }
                    /*}}}*/

                  default:
                    break;
                }
              }
              /*}}}*/

            default:
              break;
          }
        }

    }
  }
  /*}}}*/

  else if(instructionBits[4] > 0)/*{{{*/
  {
    switch(instructionBits[5])
    {
      case 0: /*{{{*/
        {
          switch(instructionBits[4])
          {
            case 1:
              { // MOV src, dst (src) -> (dst)
                src_temp = EA(address(src));  // Get effective address of src
                tmp = memory->Read(src_temp); // Get value at address of src
                dst_temp = EA(address(dst));  // Get effective address of dst
                memory->Write(dst_temp,tmp);   // Write value to memory
                resultIsZero(tmp);            // Update Z bit
                resultLTZero(tmp);            // Update N bit
                update_flags(0,Vbit);         // Update V bit
                return instruction;
              }
              
            case 2:
              { // CMP src, dst (src) + ~(dst) + 1
                src_temp = EA(address(src));  // Get effective address of src
                dst_temp = EA(address(dst));  // Get effective address of dst
                tmp = memory->Read(src_temp); // Get value at address of src
                tmp = tmp + ~(memory->Read(dst_temp)) + 1; // Compare values
                resultIsZero(tmp);            // Update Z bit
                resultLTZero(tmp);            // Update N bit
                (tmp & 0x80) > 0? update_flags(0,Cbit) : update_flags(1,Cbit); // Update C bit
                (((memory->Read(src_temp) & 0x80) ^ (memory->Read(dst_temp) & 0x80)) & \
                 ~((memory->Read(dst_temp) & 0x80) ^ (tmp & 0x80))) == 0? \
                  update_flags(1,Vbit) : update_flags(0,Vbit); // Update V bit
                return instruction;

              }

            case 3:
              { // BIT src, dst - src & dst
                src_temp = EA(address(src));  // Get effective address of src
                dst_temp = EA(address(dst));  // Get effective address of dst
                tmp = memory->Read(src_temp) & memory->Read(dst_temp); // Get test value
                resultIsZero(tmp);            // Update Z bit
                resultIsZero(tmp & 0x80);     // Update N bit
                update_flags(0,Vbit);         // Update V bit
                return instruction;
              }

            case 4:
              return instruction;  // BIC src, dst

            case 5:
              return instruction;  // BIS src, dst

            case 6:
              { // ADD src, dst - (src) + (dst) -> (dst)
                src_temp = EA(address(src));  // Get effective address of src
                dst_temp = EA(address(dst));  // Get effective address of dst
                tmp = memory->Read(src_temp) + memory->Read(dst_temp);  // Add src and dst
                memory->Write(dst_temp,tmp);  // Write result to memory
                resultIsZero(tmp);            // Update Z bit
                resultLTZero(tmp);            // Update N bit
                ((memory->Read(dst_temp) & 0x80) | (memory->Read(src_temp) & 0x80)) \
                  && (tmp > 0)? update_flags(1,Cbit) : update_flags(0,Cbit);  // Update C bit
                // Update V bit
                return instruction;
              }

            default:
              break;
          }
        }
        /*}}}*/

      case 1: /*{{{*/
        {
          switch(instructionBits[4])
          {
            case 1:
              return instruction;  // MOVB src, dst

            case 2:
              return instruction;  // CMPB src, dst

            case 3:
              return instruction;  // BITB src, dst

            case 4:
              return instruction;  // BICB src, dst

            case 5:
              return instruction;  // BISB src, dst

            case 6:
              { // SUB src, dst - (dst) + ~(src) -> (dst)
                src_temp = EA(address(src));  // Get effective address of src
                dst_temp = EA(address(dst));  // Get effective address of dst
                tmp = memory->Read(dst_temp); // Get value of dst
                tmp = tmp + ~(memory->Read(src_temp)) + 1;  // Subtract
                memory->Write(dst_temp, tmp); // Write to memory
                resultIsZero(tmp);            // Update Z bit
                resultLTZero(tmp);            // Update N bit
                // Update C bit
                // Update V bit
                return instruction;
              }

            default:
              break;
          }
        }
        /*}}}*/

      default:
        break;
    }
  }
  /*}}}*/
  /*}}}*/

  // This branch should never be reached
  if (debugLevel == Verbosity::minimal || debugLevel == Verbosity::verbose)
  {
    std::cout << "Control fell through to bottom of execute function!" << std::endl;
  }

  return -2;
}
/*}}}*/

void CPU::SetDebugMode(Verbosity verbosity)/*{{{*/
{
  this->debugLevel = verbosity;
  return;
}
/*}}}*/
