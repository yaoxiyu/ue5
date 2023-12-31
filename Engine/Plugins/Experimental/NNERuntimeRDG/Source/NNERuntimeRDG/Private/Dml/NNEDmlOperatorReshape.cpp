// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "Algo/Transform.h"
#include "Algo/Count.h"
#include "Misc/EnumerateRange.h"
#include "Helper/NNERuntimeRDGHelperReshape.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

/**
 * Reshape
 */
class FOperatorDmlReshape : public FOperatorDml
{

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlReshape();
	}

    static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		//TODO
		return true;
	}

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNE::Internal::FTensor> InputTensors, TArrayView<const NNE::Internal::FTensor> OutputTensors, const NNE::FAttributeMap& Attributes) override
	{
        check(InputTensors.Num() == 2);
        check(OutputTensors.Num() == 1);

        ConstantCPUInputs.Add(1);

        // Shape tensor must be constant!
        check(InputTensors[1].HasPreparedData());

        bool bAllowZero = (bool)
			( Attributes.GetValueOrDefault<int32>(TEXT("allowzero"), 0) );

        Util::FSmallUIntArray ReshapedShape;

        switch(InputTensors[1].GetDataType())
        {
        case ENNETensorDataType::Int32:
            if(!ShapeHelper::Reshape::ReshapeTensor<int32>(InputTensors[0].GetShape(), InputTensors[1], bAllowZero, ReshapedShape))
            {
                return false;
            }
            break;
        case ENNETensorDataType::Int64:
			if (!ShapeHelper::Reshape::ReshapeTensor<int64>(InputTensors[0].GetShape(), InputTensors[1], bAllowZero, ReshapedShape))
            {
                return false;
            }
            break;
        case ENNETensorDataType::UInt32:
			if (!ShapeHelper::Reshape::ReshapeTensor<uint32>(InputTensors[0].GetShape(), InputTensors[1], bAllowZero, ReshapedShape))
            {
                return false;
            }
            break;
        default:
            UE_LOG(LogNNE, Warning, TEXT("Shape tensor has invalid data type"));
			return false;
        };
        
        check(ReshapedShape == OutputTensors[0].GetShape().GetData());

        FTensorDescDml DmlInputTensorDesc;

        if (!DmlInputTensorDesc
				.SetFromTensor(InputTensors[0])
				.SetShape(ReshapedShape)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Reshape's input tensor for DML inference"));
			return false;
		}

        FTensorDescDml DmlOutputTensorDesc;

        if (!DmlOutputTensorDesc
				.SetFromTensor(OutputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Reshape's output tensor for DML inference"));
			return false;
		}
        
		DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC DmlIdentityOpDesc{};

        DmlIdentityOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
        DmlIdentityOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_ELEMENT_WISE_IDENTITY, &DmlIdentityOpDesc} );
	}
};

// Register Reshape operator on Module startup
NNE_DML_REGISTER_OP(Reshape)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
