/*!
	\file		CardBase.cpp
	\copyright	(c) Kaido Kert ( kaidokert@gmail.com )    
	\licence	BSD
	\author		$Author: kaidokert $
	\date		$Date: 2009-07-15 21:16:04 +0300 (Wed, 15 Jul 2009) $
*/
// Revision $Revision: 361 $
#include "CardBase.h"
#include <algorithm>
#include "helperMacro.h"
#include "common.h"

CardError::CardError(byte a,byte b):runtime_error("invalid condition on card")
	,SW1(a),SW2(b) {
	std::ostringstream buf;
	buf << "CardError:'" << runtime_error::what() << "'" << 
		" SW1:'0x" <<
		std::hex << std::setfill('0') <<
		std::setw(2) << ushort(a) << "'"
		" SW2:'0x" <<
		std::hex << std::setfill('0') <<
		std::setw(2) << ushort(b) << "'"
		;
	desc = buf.str();
}

CardBase::CardBase(ManagerInterface &ref) :
	mManager(ref),mConnection(NULL),mLogger(ref.mLogger) {
	}

CardBase::CardBase(ManagerInterface &ref,unsigned int idx) :
	mManager(ref),mLogger(ref.mLogger)
{
	connect(idx);
}

CardBase::CardBase(ManagerInterface &ref,ConnectionBase *conn):
	mManager(ref),mConnection(conn),mLogger(ref.mLogger) {}

void CardBase::connect(unsigned int idx) {
	mConnection = mManager.connect(idx);
	reIdentify();
}

CardBase::~CardBase(void)
{
	if (mConnection) {
		delete mConnection;
		mConnection = NULL;
		}
}

ByteVec CardBase::getTag(int identTag,int len,ByteVec &arr) {
	std::ostringstream oss;
	ByteVec::iterator iTag;
	iTag = find(arr.begin(),arr.end(),identTag);
	if (iTag == arr.end() ) {
		oss <<  "fci tag not found, tag " << identTag;
		throw CardDataError( oss.str() );
		}
	if (len && *(iTag+1) != len) {
		oss <<  "fci tag " << identTag << " invalid length, expecting " <<
			len << " got " << int(*(iTag+1));
		throw CardDataError(oss.str());
		}

	return ByteVec(iTag + 2,iTag + 2 + *(iTag + 1));
	}

CardBase::FCI CardBase::parseFCI(ByteVec fci) {
	throw std::runtime_error("unimplemented");
}


CardBase::FCI CardBase::selectMF(bool ignoreFCI)
{
	byte cmdMF[]= {CLAByte(), 0xA4, 0x00, ignoreFCI ? 0x08 : 0x00, 0x00/*0x02,0x3F,0x00*/}; 
	ByteVec code;
	code = execute( MAKEVECTOR(cmdMF));
	if (ignoreFCI) return FCI();
	return parseFCI(code);;
}

int CardBase::selectDF(int fileID,bool ignoreFCI)
{
	byte cmdSelectDF[] = {CLAByte(),0xA4,0x01,ignoreFCI ? 0x08 : 0x04,0x02};
	ByteVec cmd(MAKEVECTOR(cmdSelectDF));
	cmd.push_back(HIBYTE(fileID));
	cmd.push_back(LOBYTE(fileID));
	ByteVec fcp =  execute(cmd);
	if (ignoreFCI) return 0;
/*	FCI blah = */parseFCI(fcp);
	return 0;
}

CardBase::FCI CardBase::selectEF(int fileID,bool ignoreFCI)
{
	byte cmdSelectEF[] = {CLAByte(), 0xA4, 0x02, ignoreFCI ? 0x08 : 0x04, 0x02 };
	ByteVec cmd(MAKEVECTOR(cmdSelectEF));
	cmd.push_back(HIBYTE(fileID));
	cmd.push_back(LOBYTE(fileID));
	ByteVec fci = execute(cmd);

	if (ignoreFCI)
		return FCI();
	return parseFCI(fci);
}

#define PACKETSIZE 254

ByteVec CardBase::readEF(unsigned int  fileLen) 
{
	byte cmdReadEF[] = {CLAByte(),0xB0,0x00,0x00,0x00};
	ByteVec cmd(MAKEVECTOR(cmdReadEF));

	ByteVec read(0);
	dword i=0;
	
	do {
		byte bytes = LOBYTE( i + PACKETSIZE > fileLen ? fileLen - i : PACKETSIZE );
		
		cmd[2] = HIBYTE(i); //offsethi
		cmd[3] = LOBYTE(i); //offsetlo
		cmd[4] = bytes; //count

		ByteVec ret = execute(cmd,true);
		if ( bytes != ret.size() ) 
			throw CardDataError("less bytes read from binary file than specified");

		read.insert(read.end(), ret.begin(),ret.end());
		i += PACKETSIZE ;
	} while (i < (fileLen - 1));
	return read;
}

ByteVec CardBase::readRecord(int numrec) 
{
	byte cmdReadREC[] = {CLAByte(),0xB2,0x00,0x04,0x00}; 

	cmdReadREC[2] = LOBYTE(numrec);
	return execute(MAKEVECTOR(cmdReadREC),true);
}

void CardBase::executePinEntry(ByteVec cmd) {
	mManager.execPinEntryCommand(mConnection,cmd);
	}

void CardBase::executePinChange(ByteVec cmd, size_t oldPinLen,size_t newPinLen) {
	mManager.execPinChangeCommand(mConnection,cmd,oldPinLen,newPinLen);
	}

void CardBase::setLogging(std::ostream *logStream) {
	mLogger = logStream;
	}

ByteVec CardBase::execute(ByteVec cmd,bool noreply)
{
	ByteVec RecvBuffer(1024);
	uint realLen = (uint) RecvBuffer.size() ;

	/*
	if (mManager.isT1Protocol(mConnection) && !noreply) {
		cmd.push_back((byte)realLen);
		}
		*/

	if (mLogger != 0 && mLogger->good()) {
		*mLogger << "-> " << std::setfill('0') << std::setw(2) << cmd.size();   
		if (mManager.isT1Protocol(mConnection))
		   	*mLogger << " (T1):   ";
		else
		   	*mLogger << " (T0):   ";		
		for (ByteVec::iterator it = cmd.begin(); it < cmd.end(); it++)
			*mLogger << std::hex << std::setfill('0') << std::setw(2) <<  (int) *it << " ";

		*mLogger << std::endl << std::endl;
	}

	mManager.execCommand(mConnection, cmd, RecvBuffer, realLen);

	if (realLen < 2)
		// FIXME
		throw std::runtime_error("zero-length input from cardmanager");

	byte SW1 = RecvBuffer[ realLen - 2 ];
	byte SW2 = RecvBuffer[ realLen - 1 ];

	/*
	if (SW1 == 0x67 ) { //fallback, this should never occur in production
		cmd.pop_back();
		realLen = (dword) RecvBuffer.size();
		mManager.execCommand(mConnection,cmd,RecvBuffer,realLen);
		if (realLen < 2)
			// FIXME
			throw std::runtime_error("zero-length input from cardmanager");
		SW1 = RecvBuffer[ realLen - 2 ];
		SW2 = RecvBuffer[ realLen - 1 ];
		}
	*/

	RecvBuffer.resize(realLen - 2);

	if (mLogger != 0 && mLogger->good()) {

		*mLogger << "<- ";
		*mLogger << std::hex << std::setfill('0') << std::setw(2) <<  (int) SW1;
		*mLogger << std::hex << std::setfill('0') << std::setw(2) <<  (int) SW2;
		if (RecvBuffer.size() > 0) {
			*mLogger << " [" << std::hex << std::setfill('0') << std::setw(2) <<  RecvBuffer.size() << "]: ";
			for(ByteVec::iterator it = RecvBuffer.begin(); it < RecvBuffer.end(); it++ )
				*mLogger << std::hex << std::setfill('0') << std::setw(2) <<  (int) *it << " ";
		}
		*mLogger << std::endl << std::endl;
	}

	if (SW1 == 0x61) {
		byte cmdRead[]= {CLAByte(),0xC0,0x00,0x00,0x00};
		cmdRead[4] = SW2;
		return execute(MAKEVECTOR(cmdRead), true);
	}

	if (SW1 != 0x90)
		throw CardError(SW1,SW2);

	return RecvBuffer;
}


void CardBase::endTransaction() {
	mManager.endTransaction(mConnection,true);
	}

bool CardBase::hasSecurePinEntry() {
	return mConnection->isSecure();
	}
