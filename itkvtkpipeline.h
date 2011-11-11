///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: pipeline.h
// Purpose: itk-vtk and vtk-itk pipeline templates.
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// itk includes
#include <itkVTKImageExport.h>
#include <itkVTKImageImport.h>
#include <itkSmartPointer.h>

// vtk includes
#include <vtkImageExport.h>
#include <vtkImageImport.h>
#include <vtkSmartPointer.h>

// itk to vtk connexion
template<typename ITK_Exporter, typename VTK_Importer> void ConnectPipelines(itk::SmartPointer<ITK_Exporter> exporter, vtkSmartPointer<VTK_Importer> importer)
{
	importer->SetUpdateInformationCallback(exporter->GetUpdateInformationCallback());

	importer->SetPipelineModifiedCallback(exporter->GetPipelineModifiedCallback());
	importer->SetWholeExtentCallback(exporter->GetWholeExtentCallback());
	importer->SetSpacingCallback(exporter->GetSpacingCallback());
	importer->SetOriginCallback(exporter->GetOriginCallback());
	importer->SetScalarTypeCallback(exporter->GetScalarTypeCallback());
	importer->SetNumberOfComponentsCallback(exporter->GetNumberOfComponentsCallback());
	importer->SetPropagateUpdateExtentCallback(exporter->GetPropagateUpdateExtentCallback());
	importer->SetUpdateDataCallback(exporter->GetUpdateDataCallback());
	importer->SetDataExtentCallback(exporter->GetDataExtentCallback());
	importer->SetBufferPointerCallback(exporter->GetBufferPointerCallback());
	importer->SetCallbackUserData(exporter->GetCallbackUserData());
}

// vtk to itk connexion
template<typename VTK_Exporter,typename ITK_Importer> void ConnectPipelines(vtkSmartPointer<VTK_Exporter> exporter, itk::SmartPointer<ITK_Importer> importer)
{
	importer->SetUpdateInformationCallback(exporter->GetUpdateInformationCallback());

	importer->SetPipelineModifiedCallback(exporter->GetPipelineModifiedCallback());
	importer->SetWholeExtentCallback(exporter->GetWholeExtentCallback());
	importer->SetSpacingCallback(exporter->GetSpacingCallback());
	importer->SetOriginCallback(exporter->GetOriginCallback());
	importer->SetScalarTypeCallback(exporter->GetScalarTypeCallback());
	importer->SetNumberOfComponentsCallback(exporter->GetNumberOfComponentsCallback());
	importer->SetPropagateUpdateExtentCallback(exporter->GetPropagateUpdateExtentCallback());
	importer->SetUpdateDataCallback(exporter->GetUpdateDataCallback());
	importer->SetDataExtentCallback(exporter->GetDataExtentCallback());
	importer->SetBufferPointerCallback(exporter->GetBufferPointerCallback());
	importer->SetCallbackUserData(exporter->GetCallbackUserData());
}
