﻿// Copyright JAA Contributors 2024-2025

#pragma once

#include "../Constructor/Importer.h"

class UCurveLinearColorImporter : public IImporter {
public:
	UCurveLinearColorImporter(const FString& FileName, const FString& FilePath, const TSharedPtr<FJsonObject>& JsonObject, UPackage* Package, UPackage* OutermostPkg):
		IImporter(FileName, FilePath, JsonObject, Package, OutermostPkg) {
	}

	virtual bool ImportData() override;
};