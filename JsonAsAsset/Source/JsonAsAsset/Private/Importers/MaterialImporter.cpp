// Copyright Epic Games, Inc. All Rights Reserved.

#include "Importers/MaterialImporter.h"

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3) || ENGINE_MAJOR_VERSION == 4
#include "Materials/Material.h"
#else
#include "MaterialDomain.h"
#endif
#include "Dom/JsonObject.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditor/Private/MaterialEditor.h"
#include "MaterialGraph/MaterialGraph.h"
#include <Editor/UnrealEd/Classes/MaterialGraph/MaterialGraphNode_Composite.h>
#include <Editor/UnrealEd/Classes/MaterialGraph/MaterialGraphSchema.h>

void UMaterialImporter::ComposeExpressionPinBase(UMaterialExpressionPinBase* Pin, TMap<FName, UMaterialExpression*>& CreatedExpressionMap, const TSharedPtr<FJsonObject>& _JsonObject, TMap<FName, FImportData>& Exports) {
	FJsonObject* Expression = (Exports.Find(GetExportNameOfSubobject(_JsonObject->GetStringField("ObjectName")))->Json)->GetObjectField("Properties").Get();

	Pin->GraphNode->NodePosX = Expression->GetNumberField("MaterialExpressionEditorX");
	Pin->GraphNode->NodePosY = Expression->GetNumberField("MaterialExpressionEditorY");
	Pin->MaterialExpressionEditorX = Expression->GetNumberField("MaterialExpressionEditorX");
	Pin->MaterialExpressionEditorY = Expression->GetNumberField("MaterialExpressionEditorY");

	FString MaterialExpressionGuid;
	if (Expression->TryGetStringField("MaterialExpressionGuid", MaterialExpressionGuid)) Pin->MaterialExpressionGuid = FGuid(MaterialExpressionGuid);

	const TArray<TSharedPtr<FJsonValue>>* ReroutePins;
	if (Expression->TryGetArrayField("ReroutePins", ReroutePins)) {
		for (const TSharedPtr<FJsonValue> ReroutePin : *ReroutePins) {
			if (ReroutePin->IsNull()) continue;
			TSharedPtr<FJsonObject> ReroutePinObject = ReroutePin->AsObject();
			TSharedPtr<FJsonObject> RerouteObj = GetExportByObjectPath(ReroutePinObject.Get()->GetObjectField("Expression"))->AsObject();

			UMaterialExpressionReroute* RerouteExpression = Cast<UMaterialExpressionReroute>(*CreatedExpressionMap.Find(FName(RerouteObj->GetStringField("Name"))));

			// Add reroute to pin
			Pin->ReroutePins.Add(FCompositeReroute(FName(*ReroutePinObject->GetStringField("Name")), RerouteExpression));
		}
	}

	// Set Pin Direction
	FString PinDirection;
	if (Expression->GetObjectField("Properties")->TryGetStringField("PinDirection", PinDirection)) {
		EEdGraphPinDirection Enum_PinDirection = EGPD_Input;

		if (PinDirection.EndsWith("EGPD_Input")) Enum_PinDirection = EGPD_Input;
		else if (PinDirection.EndsWith("EGPD_Output")) Enum_PinDirection = EGPD_Output;

		Pin->PinDirection = Enum_PinDirection;
	}

	const TArray<TSharedPtr<FJsonValue>>* OutputsPtr;
	if (Expression->TryGetArrayField("Outputs", OutputsPtr)) {
		TArray<FExpressionOutput> Outputs;
		for (const TSharedPtr<FJsonValue> OutputValue : *OutputsPtr) {
			TSharedPtr<FJsonObject> OutputObject = OutputValue->AsObject();
			Outputs.Add(PopulateFuncExpressionOutput(OutputObject).Output);
		}

		Pin->Outputs = Outputs;
	}

	Pin->MarkPackageDirty();
	Pin->Modify();
}

bool UMaterialImporter::ImportData() {
	try {
		// Create Material Factory (factory automatically creates the M)
		UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
		UMaterial* Material = Cast<UMaterial>(MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), OutermostPkg, *FileName, RF_Standalone | RF_Public, nullptr, GWarn));

		Material->StateId = FGuid(JsonObject->GetObjectField("Properties")->GetStringField("StateId"));
		bool bCanMaskedBeAssumedOpaque;
		if (JsonObject->GetObjectField("Properties")->TryGetBoolField("bCanMaskedBeAssumedOpaque", bCanMaskedBeAssumedOpaque)) Material->bCanMaskedBeAssumedOpaque = bCanMaskedBeAssumedOpaque;
		FString MaterialDomainString;
		if (JsonObject->GetObjectField("Properties")->TryGetStringField("MaterialDomain", MaterialDomainString)) {
			EMaterialDomain MaterialDomain = MD_Surface;

			if (MaterialDomainString.EndsWith("MD_DeferredDecal")) MaterialDomain = MD_DeferredDecal;
			else if (MaterialDomainString.EndsWith("MD_LightFunction")) MaterialDomain = MD_LightFunction;
			else if (MaterialDomainString.EndsWith("MD_Volume")) MaterialDomain = MD_Volume;
			else if (MaterialDomainString.EndsWith("MD_PostProcess")) MaterialDomain = MD_PostProcess;
			else if (MaterialDomainString.EndsWith("MD_UI")) MaterialDomain = MD_UI;

			Material->MaterialDomain = MaterialDomain;
		}

		FString BlendModeString;
		if (JsonObject->GetObjectField("Properties")->TryGetStringField("BlendMode", BlendModeString)) {
			EBlendMode BlendMode = BLEND_Translucent;

			if (BlendModeString.EndsWith("BLEND_Masked")) BlendMode = BLEND_Masked;
			else if (BlendModeString.EndsWith("BLEND_Translucent")) BlendMode = BLEND_Translucent;
			else if (BlendModeString.EndsWith("BLEND_Additive")) BlendMode = BLEND_Additive;
			else if (BlendModeString.EndsWith("BLEND_Modulate")) BlendMode = BLEND_Modulate;
			else if (BlendModeString.EndsWith("BLEND_AlphaComposite")) BlendMode = BLEND_AlphaComposite;
			else if (BlendModeString.EndsWith("BLEND_AlphaHoldout")) BlendMode = BLEND_AlphaHoldout;

			Material->BlendMode = BlendMode;
		}

		FString ShadingModelString;
		if (JsonObject->GetObjectField("Properties")->TryGetStringField("ShadingModel", ShadingModelString)) {
			EMaterialShadingModel ShadingModel = MSM_DefaultLit;

			if (ShadingModelString.EndsWith("MSM_Unlit")) ShadingModel = MSM_Unlit;
			else if (ShadingModelString.EndsWith("MSM_Subsurface")) ShadingModel = MSM_Subsurface;
			else if (ShadingModelString.EndsWith("MSM_PreintegratedSkin")) ShadingModel = MSM_PreintegratedSkin;
			else if (ShadingModelString.EndsWith("MSM_ClearCoat")) ShadingModel = MSM_ClearCoat;
			else if (ShadingModelString.EndsWith("MSM_SubsurfaceProfile")) ShadingModel = MSM_SubsurfaceProfile;
			else if (ShadingModelString.EndsWith("MSM_TwoSidedFoliage")) ShadingModel = MSM_TwoSidedFoliage;
			else if (ShadingModelString.EndsWith("MSM_Hair")) ShadingModel = MSM_Hair;
			else if (ShadingModelString.EndsWith("MSM_Cloth")) ShadingModel = MSM_Cloth;
			else if (ShadingModelString.EndsWith("MSM_Eye")) ShadingModel = MSM_Eye;
			else if (ShadingModelString.EndsWith("MSM_SingleLayerWater")) ShadingModel = MSM_SingleLayerWater;
			else if (ShadingModelString.EndsWith("MSM_ThinTranslucent")) ShadingModel = MSM_ThinTranslucent;
			else if (ShadingModelString.EndsWith("MSM_FromMaterialExpression")) ShadingModel = MSM_FromMaterialExpression;

			Material->SetShadingModel(ShadingModel);

			const TSharedPtr<FJsonObject>* ShadingModelsPtr;
			if (JsonObject->GetObjectField("Properties")->TryGetObjectField("ShadingModels", ShadingModelsPtr)) {
				int ShadingModelField;
				if (ShadingModelsPtr->Get()->TryGetNumberField("ShadingModelField", ShadingModelField)) Material->GetShadingModels().SetShadingModelField(ShadingModelField);
			}
		}

		bool bEnableResponsiveAA;
		if (JsonObject->GetObjectField("Properties")->TryGetBoolField("bEnableResponsiveAA", bEnableResponsiveAA)) Material->bEnableResponsiveAA = bEnableResponsiveAA;
		bool bUsedWithSkeletalMesh;
		if (JsonObject->GetObjectField("Properties")->TryGetBoolField("bUsedWithSkeletalMesh", bUsedWithSkeletalMesh)) Material->bUsedWithSkeletalMesh = bUsedWithSkeletalMesh;
		bool bUsedWithParticleSprites;
		if (JsonObject->GetObjectField("Properties")->TryGetBoolField("bUsedWithParticleSprites", bUsedWithParticleSprites)) Material->bUsedWithParticleSprites = bUsedWithParticleSprites;

		// Handle edit changes, and add it to the content browser
		if (!HandleAssetCreation(Material)) return false;

		// Clear any default expressions the engine adds (ex: Result)
		Material->GetExpressionCollection().Empty();

		// Define editor only data from the JSON
		TMap<FName, FImportData> Exports;
		TArray<FName> ExpressionNames;
		TSharedPtr<FJsonObject> EdProps = FindEditorOnlyData(JsonObject->GetStringField("Type"), Material->GetName(), Exports, ExpressionNames)->GetObjectField("Properties");

		const TSharedPtr<FJsonObject> StringExpressionCollection = EdProps->GetObjectField("ExpressionCollection");

		// const TArray<TSharedPtr<FJsonValue>>* StringExpressions;
		// if (StringExpressionCollection->TryGetArrayField("Expressions", StringExpressions)) {
		// 	for (const TSharedPtr<FJsonValue> Expression : *StringExpressions) {
		// 		if (Expression->IsNull()) continue;
		// 		ExpressionNames.Add(GetExportNameOfSubobject(Expression->AsObject()->GetStringField("ObjectName")));
		// 	}
		// }

		// Map out each expression for easier access
		TMap<FName, UMaterialExpression*> CreatedExpressionMap = CreateExpressions(Material, Material->GetName(), ExpressionNames, Exports);

		const TSharedPtr<FJsonObject>* EmissiveColorPtr;
		if (EdProps->TryGetObjectField("EmissiveColor", EmissiveColorPtr) && EmissiveColorPtr != nullptr) {
			FJsonObject* EmissiveColorObject = EmissiveColorPtr->Get();
			FName EmissiveColorExpressionName = GetExpressionName(EmissiveColorObject);

			if (CreatedExpressionMap.Contains(EmissiveColorExpressionName)) {
				FExpressionInput Ex = PopulateExpressionInput(EmissiveColorObject, *CreatedExpressionMap.Find(EmissiveColorExpressionName), "Color");
				FColorMaterialInput* EmissiveColor = reinterpret_cast<FColorMaterialInput*>(&Ex);
				Material->GetEditorOnlyData()->EmissiveColor = *EmissiveColor;
			}
		}

		const TSharedPtr<FJsonObject>* OpacityPtr;
		if (EdProps->TryGetObjectField("Opacity", OpacityPtr) && OpacityPtr != nullptr) {
			FJsonObject* OpacityObject = OpacityPtr->Get();
			FName OpacityExpressionName = GetExpressionName(OpacityObject);

			if (CreatedExpressionMap.Contains(OpacityExpressionName)) {
				FExpressionInput Ex = PopulateExpressionInput(OpacityObject, *CreatedExpressionMap.Find(OpacityExpressionName), "Scalar");
				FScalarMaterialInput* Opacity = reinterpret_cast<FScalarMaterialInput*>(&Ex);
				Material->GetEditorOnlyData()->Opacity = *Opacity;
			}
		}

		const TSharedPtr<FJsonObject>* OpacityMaskPtr;
		if (EdProps->TryGetObjectField("OpacityMask", OpacityMaskPtr) && OpacityMaskPtr != nullptr) {
			FJsonObject* OpacityMaskObject = OpacityMaskPtr->Get();
			FName OpacityMaskExpressionName = GetExpressionName(OpacityMaskObject);

			if (CreatedExpressionMap.Contains(OpacityMaskExpressionName)) {
				FExpressionInput Ex = PopulateExpressionInput(OpacityMaskObject, *CreatedExpressionMap.Find(OpacityMaskExpressionName), "Scalar");
				FScalarMaterialInput* OpacityMask = reinterpret_cast<FScalarMaterialInput*>(&Ex);
				Material->GetEditorOnlyData()->OpacityMask = *OpacityMask;
			}
		}

		const TSharedPtr<FJsonObject>* WorldPositionOffsetPtr;
		if (EdProps->TryGetObjectField("WorldPositionOffset", WorldPositionOffsetPtr) && WorldPositionOffsetPtr != nullptr) {
			FJsonObject* WorldPositionOffsetObject = WorldPositionOffsetPtr->Get();
			FName WorldPositionOffsetExpressionName = GetExpressionName(WorldPositionOffsetObject);

			if (CreatedExpressionMap.Contains(WorldPositionOffsetExpressionName)) {
				FExpressionInput Ex = PopulateExpressionInput(WorldPositionOffsetObject, *CreatedExpressionMap.Find(WorldPositionOffsetExpressionName), "Vector");
				FVectorMaterialInput* WorldPositionOffset = reinterpret_cast<FVectorMaterialInput*>(&Ex);
				Material->GetEditorOnlyData()->WorldPositionOffset = *WorldPositionOffset;
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* StringParameterGroupData;
		if (EdProps->TryGetArrayField("ParameterGroupData", StringParameterGroupData)) {
			TArray<FParameterGroupData> ParameterGroupData;

			for (const TSharedPtr<FJsonValue> ParameterGroupDataObject : *StringParameterGroupData) {
				if (ParameterGroupDataObject->IsNull()) continue;
				FParameterGroupData GroupData;

				FString GroupName;
				if (ParameterGroupDataObject->AsObject()->TryGetStringField("GroupName", GroupName)) GroupData.GroupName = GroupName;
				int GroupSortPriority;
				if (ParameterGroupDataObject->AsObject()->TryGetNumberField("GroupSortPriority", GroupSortPriority)) GroupData.GroupSortPriority = GroupSortPriority;

				ParameterGroupData.Add(GroupData);
			}

			Material->GetEditorOnlyData()->ParameterGroupData = ParameterGroupData;
		}

		// Iterate through all the expression names
		AddExpressions(Material, ExpressionNames, Exports, CreatedExpressionMap, true);
		AddComments(Material, StringExpressionCollection, Exports);

		// Create Editor
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		FMaterialEditor* AssetEditorInstance = reinterpret_cast<FMaterialEditor*>(AssetEditorSubsystem->OpenEditorForAsset(Material) ? AssetEditorSubsystem->FindEditorForAsset(Material, true) : nullptr);

		for (const TSharedPtr<FJsonValue> Value : AllJsonObjects) {
			TSharedPtr<FJsonObject> Object = TSharedPtr(Value->AsObject());

			FString ExType = Object->GetStringField("Type");
			FString Name = Object->GetStringField("Name");

			if (ExType == "MaterialGraph" && Name != "MaterialGraph_0") {
				TSharedPtr<FJsonObject> Properties = Object->GetObjectField("Properties");
				TSharedPtr<FJsonObject> SubgraphExpression;

				FString SubgraphExpressionName;

				// Set SubgraphExpression
				const TSharedPtr<FJsonObject>* SubgraphExpressionPtr = nullptr;
				if (Properties->TryGetObjectField("SubgraphExpression", SubgraphExpressionPtr) && SubgraphExpressionPtr != nullptr) {
					FJsonObject* SubgraphExpressionObject = SubgraphExpressionPtr->Get();
					FName ExportName = GetExportNameOfSubobject(SubgraphExpressionObject->GetStringField("ObjectName"));

					SubgraphExpressionName = ExportName.ToString();
					FImportData Export = *Exports.Find(ExportName);
					SubgraphExpression = Export.Json->GetObjectField("Properties");
				}

				// Find Material Graph
				UMaterialGraph* MaterialGraph = AssetEditorInstance->Material->MaterialGraph;
				if (MaterialGraph == nullptr) {
					UE_LOG(LogJson, Log, TEXT("The mat graph not valid!"))
				}

				MaterialGraph->Modify();

				// Create the composite node that will serve as the gateway into the subgraph
				UMaterialGraphNode_Composite* GatewayNode = nullptr;
				{
					GatewayNode = Cast<UMaterialGraphNode_Composite>(FMaterialGraphSchemaAction_NewComposite::SpawnNode(MaterialGraph, FVector2D(SubgraphExpression->GetNumberField("MaterialExpressionEditorX"), SubgraphExpression->GetNumberField("MaterialExpressionEditorY"))));
					GatewayNode->bCanRenameNode = true;
					check(GatewayNode);
				}

				UMaterialGraph* DestinationGraph = Cast<UMaterialGraph>(GatewayNode->BoundGraph);
				UMaterialExpressionComposite* CompositeExpression = CastChecked<UMaterialExpressionComposite>(GatewayNode->MaterialExpression);
				{
					CompositeExpression->Material = Material;
					CompositeExpression->SubgraphName = Name;

					AddGenerics(Material, CompositeExpression, SubgraphExpression);
				}

				// Add Sub-Graph Nodes
				{
					TArray<TSharedPtr<FJsonValue>> MaterialGraphNodes = FilterGraphNodesBySubgraphExpression(SubgraphExpressionName);
					TMap<FName, FImportData> SubGraphExports;
					TMap<FName, UMaterialExpression*> SubgraphExpressionMapping;
					TArray<FName> SubGraphExpressionNames;

					// Go through each expression
					for (const TSharedPtr<FJsonValue> _GraphNode : MaterialGraphNodes) {
						const TSharedPtr<FJsonObject> MaterialGraphObject = TSharedPtr(_GraphNode->AsObject());

						FString GraphNode_Type = MaterialGraphObject->GetStringField("Type");
						FString GraphNode_Name = MaterialGraphObject->GetStringField("Name");

						FName GraphNodeNameName = FName(GraphNode_Name);

						UMaterialExpression* Ex = CreateEmptyExpression(Material, GraphNodeNameName, FName(GraphNode_Type));
						if (Ex == nullptr)
							continue;

						Ex->SubgraphExpression = CompositeExpression;
						Ex->Material = Material;

						SubGraphExpressionNames.Add(GraphNodeNameName);
						SubGraphExports.Add(GraphNodeNameName, FImportData(GraphNode_Type, Material->GetName(), MaterialGraphObject));
						SubgraphExpressionMapping.Add(GraphNodeNameName, Ex);
					}

					// Add all the expression properties
					AddExpressions(MaterialGraph, SubGraphExpressionNames, Exports, SubgraphExpressionMapping);

					// All expressions (hopefully) have their properties
					// so now we just make a material graph node for each
					for (const TPair<FName, UMaterialExpression*>& pair : SubgraphExpressionMapping) {
						UMaterialExpression* Expression = pair.Value;
						UMaterialGraphNode* NewNode = NewObject<UMaterialGraphNode>(DestinationGraph, UMaterialGraphNode::StaticClass(), NAME_None, RF_Transactional);

						NewNode->NodePosX = Expression->MaterialExpressionEditorX;
						NewNode->NodePosY = Expression->MaterialExpressionEditorY;
						NewNode->MaterialExpression = Expression;

						DestinationGraph->AddNode(NewNode);

						NewNode->ReconstructNode();
					}

					// Setup Input/Output Expressions
					{
						const TSharedPtr<FJsonObject>* InputExpressions;
						if (SubgraphExpression->TryGetObjectField("InputExpressions", InputExpressions)) {
							TSharedPtr<FJsonObject> InputExpression = TSharedPtr<FJsonObject>(InputExpressions->Get());

							ComposeExpressionPinBase(CompositeExpression->InputExpressions, CreatedExpressionMap, InputExpression, Exports);
						};

						const TSharedPtr<FJsonObject>* OutputExpressions;
						if (SubgraphExpression->TryGetObjectField("OutputExpressions", OutputExpressions)) {
							TSharedPtr<FJsonObject> OutputExpression = TSharedPtr<FJsonObject>(OutputExpressions->Get());

							ComposeExpressionPinBase(CompositeExpression->OutputExpressions, CreatedExpressionMap, OutputExpression, Exports);
						};
					}
				}

				GatewayNode->ReconstructNode();
				Cast<UMaterialGraphNode>(CompositeExpression->InputExpressions->GraphNode)->ReconstructNode();
				Cast<UMaterialGraphNode>(CompositeExpression->OutputExpressions->GraphNode)->ReconstructNode();

				// Update Original Material
				AssetEditorInstance->UpdateMaterialAfterGraphChange();
			}
		}

		AssetEditorInstance->UpdateMaterialAfterGraphChange();
		Material->PostEditChange();
	} catch (const char* Exception) {
		UE_LOG(LogJson, Error, TEXT("%s"), *FString(Exception));
		return false;
	}

	return true;
}

// Filter out Material Graph Nodes
// by checking their subgraph expression (composite)
TArray<TSharedPtr<FJsonValue>> UMaterialImporter::FilterGraphNodesBySubgraphExpression(const FString& Outer) {
	TArray<TSharedPtr<FJsonValue>> ReturnValue = TArray<TSharedPtr<FJsonValue>>();

	/*
	* How this works:
	* 1. Find a Material Graph Node
	* 2. Get the Material Expression
	* 3. Compare SubgraphExpression to the one provided
	*    to the function
	*/
	for (const TSharedPtr<FJsonValue> Value : AllJsonObjects) {
		const TSharedPtr<FJsonObject> ValueObject = TSharedPtr(Value->AsObject());
		const TSharedPtr<FJsonObject> Properties = TSharedPtr(ValueObject->GetObjectField("Properties"));

		const TSharedPtr<FJsonObject>* MaterialExpression;
		if (Properties->TryGetObjectField("MaterialExpression", MaterialExpression)) {
			TSharedPtr<FJsonValue> ExpValue = GetExportByObjectPath(*MaterialExpression);
			TSharedPtr<FJsonObject> Expression = TSharedPtr(ExpValue->AsObject());
			const TSharedPtr<FJsonObject> _Properties = TSharedPtr(Expression->GetObjectField("Properties"));

			const TSharedPtr<FJsonObject>* _SubgraphExpression;
			if (_Properties->TryGetObjectField("SubgraphExpression", _SubgraphExpression)) {
				if (Outer == GetExportNameOfSubobject(_SubgraphExpression->Get()->GetStringField("ObjectName")).ToString()) {
					ReturnValue.Add(ExpValue);
				}
			}
		}
	}

	return ReturnValue;
}
