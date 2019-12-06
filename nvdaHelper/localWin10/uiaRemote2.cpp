#include <windows.h>
#include <atlsafe.h>
#include <atlcomcli.h>
#include <UIAutomation.h>
#include <UiaOperationAbstraction/UiaOperationAbstraction.h>
#include <common/log.h>

using namespace UiaOperationAbstraction;

UiaArray<UiaTextRange> _remoteable_splitTextRangeByUnit(UiaOperationScope& scope, UiaTextRange& textRange, TextUnit unit) {
	UiaArray<UiaTextRange> ranges;
	// Start with a clone of textRange collapsed to the start
	auto tempRange=textRange.Clone();
	tempRange.MoveEndpointByRange(TextPatternRangeEndpoint_End,tempRange,TextPatternRangeEndpoint_Start);
	UiaInt endDelta=0;
	scope.While([&](){ return true; },[&](){
		// Move the end forward by the given unit
		auto moved=tempRange.MoveEndpointByUnit(TextPatternRangeEndpoint_End, unit, 1);
		scope.If(moved<=0,[&]() {
			// We couldn't move forward, so break
			scope.Break();
		});
		// Find out if we are past the end of the over all textRange yet
		endDelta = tempRange.CompareEndpoints(TextPatternRangeEndpoint_End,textRange,TextPatternRangeEndpoint_End);
		scope.If(endDelta>0,[&](){
			// We moved past the end of the over all textRange,
			// Clip the tempRange so it stays inside
			tempRange.MoveEndpointByRange(TextPatternRangeEndpoint_End,textRange,TextPatternRangeEndpoint_End);
		});
		// Save off this range
		ranges.Append(tempRange.Clone());
		scope.If(endDelta>0,[&](){
			// Since we had to clip to the end of the over all textRange,
			// We should not go any further.
			scope.Break();
		});
		// collapse the tempRange up to the end, ready to go through the loop again
		tempRange.MoveEndpointByRange(TextPatternRangeEndpoint_Start,tempRange,TextPatternRangeEndpoint_End);
	}); // loop end
	scope.If(endDelta<0,[&](){ 
		// For some reason we never reached the end of the over all textRange.
		// Move the end forward so we do.
		tempRange.MoveEndpointByRange(TextPatternRangeEndpoint_End,textRange,TextPatternRangeEndpoint_End);
		// Save off this final range.
		ranges.Append(tempRange.Clone());
	});
	return ranges;
}

const auto textContentCommand_elementStart=1;
const auto textContentCommand_text=2;
const auto textContentCommand_elementEnd=3;

UiaArray<UiaVariant> _remoteable_getTextContent(UiaOperationScope& scope, UiaTextRange& textRange, const std::vector<int>& attribIDs) {
	auto ranges=_remoteable_splitTextRangeByUnit(scope,textRange,TextUnit_Format);
	auto s=ranges.Size();
	UiaUint index=0;
	UiaArray<UiaVariant> textContent;
	scope.For([&](){},[&]() {
		return index<s;
	},[&](){
		index+=1;
	},[&]() {
		auto subrange=ranges.GetAt(index);
		textContent.Append(UiaVariant(textContentCommand_text));
		for(auto& attribID: attribIDs) {
			auto attribValue=subrange.GetAttributeValue(UiaTextAttributeId(attribID));
			textContent.Append(attribValue);
		}
		textContent.Append(UiaVariant(subrange.GetText(-1)));
	});
	return textContent;
}

extern "C" __declspec(dllexport) SAFEARRAY* __stdcall uiaRemote_getTextContent(IUIAutomationTextRange* textRangeArg, SAFEARRAY* pAttribIDsArg) {
	auto scope=UiaOperationScope::StartNew();
	UiaTextRange textRange{textRangeArg};
	CComSafeArray<int> attribIDsArray{pAttribIDsArg};
	auto attribCount=attribIDsArray.GetCount();
	std::vector<int> attribIDs(attribCount);
	for(size_t i=0;i<attribCount;++i) {
		attribIDs[i]=attribIDsArray.GetAt(i);
	}
	auto textContent=_remoteable_getTextContent(scope,textRange,attribIDs);
	scope.BindResult(textContent);
	scope.Resolve();
	size_t numItems=textContent.Size();
	CComSafeArray<VARIANT> safeArray(numItems);
	for(size_t i=0;i<numItems;++i) {
		VARIANT v=*((*textContent)[i]);
		safeArray.SetAt(i,v);
	}
	return safeArray.Detach();
}

extern "C" __declspec(dllexport) void __stdcall uiaRemote_initialize(bool doRemote, IUIAutomation* client) {
	UiaOperationAbstraction::Initialize(doRemote,client);
}