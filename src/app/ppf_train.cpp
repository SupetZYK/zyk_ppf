#include "common.h"
#include "Voxel_grid.h"
#include "PPFFeature.h"
#include "pose_cluster.h"
#include "SmartSampling.hpp"

#include <pcl/io/pcd_io.h>
std::string model_filename_;
std::string save_filename_;

//Algorithm params
bool use_cloud_resolution_  (false);
bool use_ply_filetype_  (false);
bool use_existing_normal_data_  (false);
bool x_centrosymmetric_  (false);
bool y_centrosymmetric_  (false);
bool z_centrosymmetric_  (false);
bool save_sampled_cloud_ (false);
bool normal_reorient_switch_ (false);
bool smart_sample_border_ (false);
bool show_original_model_ (false);
float model_ss_ (3.0f);
float plane_ss_ (3.0f);
float curvature_radius_(2.0f);
void showHelp(char *filename)
{
	std::cout << std::endl;
	std::cout << "***************************************************************************" << std::endl;
	std::cout << "*                                                                         *" << std::endl;
	std::cout << "*             Correspondence Grouping Tutorial - Usage Guide              *" << std::endl;
	std::cout << "*                                                                         *" << std::endl;
	std::cout << "***************************************************************************" << std::endl << std::endl;
	std::cout << "Usage: " << filename << " model_filename.pcd scene_filename.pcd [Options]" << std::endl << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "     -h:						Show this help." << std::endl;
	std::cout << "     -r:						Compute the model cloud resolution and multiply" << std::endl;
	std::cout << "     -w:						write the sampled model" << std::endl;
	std::cout << "     --ply:					Use .poly as input cloud. Default is .pcd" << std::endl;
	std::cout << "     --rn:					Reorient switch!" << std::endl;
	std::cout << "     --sp:					smart sampling borders" << std::endl;
	std::cout << "     --so:					show original model" << std::endl;
	std::cout << "     --in:					Use existing normal files" << std::endl;
	std::cout << "     --model_ss val:			Model uniform sampling radius (default 3)" << std::endl;
	std::cout << "     --plane_ss val:			Model plane feature uniform sampling radius, if not set, default same as model" << std::endl;
	std::cout << "     --curv_r val:			curvature radius" << std::endl;

}

void parseCommandLine(int argc, char *argv[])
{
	//Show help
	if (pcl::console::find_switch(argc, argv, "-h"))
	{
		showHelp(argv[0]);
		exit(0);
	}

	//Program behavior
	if (pcl::console::find_switch(argc, argv, "-r"))
	{
		use_cloud_resolution_ = true;
	}
	if (pcl::console::find_switch(argc, argv, "-w"))
	{
		save_sampled_cloud_ = true;
	}
	if (pcl::console::find_switch(argc, argv, "--ply"))
	{
		use_ply_filetype_ = true;
	}
	if (pcl::console::find_switch(argc, argv, "--rn"))
	{
		normal_reorient_switch_ = true;
	}
	if (pcl::console::find_switch(argc, argv, "--sp"))
	{
		smart_sample_border_= true;
	}
	if (pcl::console::find_switch(argc, argv, "--so"))
	{
		show_original_model_ = true;
	}
	if (pcl::console::find_switch(argc, argv, "--in"))
	{
		use_existing_normal_data_ = true;
	}
	//Model & scene filenames
	std::vector<int> filenames;
	if (use_ply_filetype_ == false)
		filenames = pcl::console::parse_file_extension_argument(argc, argv, ".pcd");
	else
		filenames = pcl::console::parse_file_extension_argument(argc, argv, ".ply");
	if (filenames.size() < 1)
	{
		std::cout << "Filenames missing.\n";
		showHelp(argv[0]);
		exit(-1);
	}
	model_filename_ = argv[filenames[0]];
	int pos = model_filename_.find_last_of('.');
	save_filename_ = model_filename_.substr(0, pos);
	save_filename_ += ".ppfs";
	//General parameters
	pcl::console::parse_argument(argc, argv, "--model_ss", model_ss_);
	plane_ss_ = model_ss_;
	pcl::console::parse_argument(argc, argv, "--plane_ss", plane_ss_);
	pcl::console::parse_argument(argc, argv, "--curv_r", curvature_radius_);


}





int
main(int argc, char *argv[])
{
	parseCommandLine(argc, argv);

	showHelp(argv[0]);
	pcl::PointCloud<PointType>::Ptr model(new pcl::PointCloud<PointType>());
	pcl::PointCloud<NormalType>::Ptr model_normals(new pcl::PointCloud<NormalType>());

	std::string fileformat;
	if (use_ply_filetype_)
	{
		fileformat = "ply";
	}
	else
	{
		fileformat = "pcd";
	}
	if (use_existing_normal_data_)
	{
		if (!readPointCloud(model_filename_, fileformat, model, model_normals) )
		{
			return(-1);
		}
	}
	else
	{
		if (!readPointCloud(model_filename_, fileformat, model) )
		{
			return(-1);
		}
	}

	//
	// show original model
	//
	if (show_original_model_)
	{

		pcl::visualization::PCLVisualizer key_visual("Original Viewr");
		key_visual.addCoordinateSystem(20);
		key_visual.addPointCloudNormals<PointType, NormalType>(model, model_normals, 1, 10, "model_normal");
		key_visual.addPointCloud(model, pcl::visualization::PointCloudColorHandlerCustom<PointType>(model, 0.0, 0.0, 255.0), "model");
		key_visual.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 4, "model_normal");
		key_visual.spin();
	}

	//
	//  Set up resolution invariance
	//

	double max_coord[3];
	double min_coord[3];
	float resolution = static_cast<float> (computeCloudResolution(model, max_coord, min_coord));
	if (use_cloud_resolution_)
	{
		if (resolution != 0.0f)
		{
			model_ss_ *= resolution;
			plane_ss_ *= resolution;
		}

	}
	std::cout << "Model resolution:       " << resolution << std::endl;
	std::cout << "Model sampling distance step:    " << model_ss_ << std::endl;

	double model_length = max_coord[0] - min_coord[0];
	double model_width = max_coord[1] - min_coord[1];
	double model_height = max_coord[2] - min_coord[2];
	Eigen::Vector3f model_approximate_center;
	model_approximate_center(0) = (max_coord[0] + min_coord[0]) / 2;
	model_approximate_center(1) = (max_coord[1] + min_coord[1]) / 2;
	model_approximate_center(2) = (max_coord[2] + min_coord[2]) / 2;

	if (normal_reorient_switch_)
	{
		for (int32_t i = 0; i < model->size(); ++i)
		{
			Eigen::Vector3f pnt_temp = model->points[i].getVector3fMap();
			Eigen::Vector3f normal_temp = model_normals->points[i].getNormalVector3fMap();
			if ((pnt_temp - model_approximate_center).dot(normal_temp) < 0)
			{
				model_normals->points[i].normal_x = -normal_temp(0);
				model_normals->points[i].normal_y = -normal_temp(1);
				model_normals->points[i].normal_z = -normal_temp(2);
			}
			model->points[i].x -= model_approximate_center(0);
			model->points[i].y -= model_approximate_center(1);
			model->points[i].z -= model_approximate_center(2);
		}
	}
	
	

	std::cout << "Model length: " << model_length << std::endl;
	std::cout << "Model width: " << model_width << std::endl;
	std::cout << "Model height: " << model_height << std::endl;

	//
	//  Compute Normals
	//

	pcl::NormalEstimationOMP<PointType, NormalType> norm_est;
	
	if (!use_existing_normal_data_)
	{
		norm_est.setKSearch(10);
		norm_est.setInputCloud(model);
		//norm_est.setViewPoint(trans(0) + model_approximate_center(0), trans(1) + model_approximate_center(1), trans(2) + model_approximate_center(2));
		norm_est.compute(*model_normals);
		cout << "Normal compute completeŁĄ" << endl;
	}

	//
	// Compute curvature
	//

	pcl::PrincipalCurvaturesEstimation<PointType, NormalType> curv_est;
	pcl::PointCloud<pcl::PrincipalCurvatures>::Ptr curv(new pcl::PointCloud<pcl::PrincipalCurvatures>());
	curv_est.setInputCloud(model);
	curv_est.setInputNormals(model_normals);
	curv_est.setRadiusSearch(curvature_radius_ * resolution);
	curv_est.compute(*curv);


	//
	// show those curvature 0 and none_zero separately
	//
	pcl::PointCloud<PointType>::Ptr pnts_zero(new pcl::PointCloud<PointType>());
	pcl::PointCloud<NormalType>::Ptr normals_zero(new pcl::PointCloud<NormalType>());
	pcl::PointCloud<PointType>::Ptr pnts_no_zero(new pcl::PointCloud<PointType>());
	pcl::PointCloud<NormalType>::Ptr normals_no_zero(new pcl::PointCloud<NormalType>());
	for (int i = 0; i < model->size(); ++i)
	{
		pcl::PrincipalCurvatures c = curv->at(i);
		if (abs(c.pc1)<0.001&&abs(c.pc2)<0.001)
		{
			pnts_zero->push_back(model->at(i));
			normals_zero->push_back(model_normals->at(i));
		}
		else
		{
			pnts_no_zero->push_back(model->at(i));
			normals_no_zero->push_back(model_normals->at(i));
		}
	}

	//
	//visualize zero
	//

	pcl::visualization::PCLVisualizer zeroVisual("zero detect");
	zeroVisual.addCoordinateSystem(20);
	zeroVisual.addPointCloud(pnts_zero, pcl::visualization::PointCloudColorHandlerCustom<PointType>(pnts_zero, 0.0, 0.0, 255.0), "zero_pnts");
	zeroVisual.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 5, "zero_pnts");
	zeroVisual.spin();

	//
	//visualize no-zero
	//

	pcl::visualization::PCLVisualizer nozeroVisual("no-zero detect");
	nozeroVisual.addCoordinateSystem(20);
	nozeroVisual.addPointCloud(pnts_no_zero, pcl::visualization::PointCloudColorHandlerCustom<PointType>(pnts_no_zero, 0.0, 0.0, 255.0), "pnts_no_zero");
	nozeroVisual.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 5, "pnts_no_zero");
	nozeroVisual.spin();

	//
	//  Downsample Clouds to Extract keypoints
	//
	pcl::PointCloud<PointType>::Ptr model_zero_curvature_keypoints(new pcl::PointCloud<PointType>());
	pcl::PointCloud<NormalType>::Ptr model_zero_curvature_keyNormals(new pcl::PointCloud<NormalType>());
	pcl::PointCloud<PointType>::Ptr model_no_zero_curvatur_keypoints(new pcl::PointCloud<PointType>());
	pcl::PointCloud<NormalType>::Ptr model_no_zero_curvature_keyNormals(new pcl::PointCloud<NormalType>());
	//smat sample
	pcl::SmartSampling<PointType, NormalType> smart_samp;
	if (smart_sample_border_)
		SmartDownSamplePointAndNormal(pnts_no_zero, normals_no_zero, model_ss_, model_no_zero_curvatur_keypoints, model_no_zero_curvature_keyNormals);
	else
		uniformDownSamplePointAndNormal(pnts_no_zero, normals_no_zero, model_ss_, model_no_zero_curvatur_keypoints, model_no_zero_curvature_keyNormals);
	if (plane_ss_>0)
		uniformDownSamplePointAndNormal(pnts_zero, normals_zero, plane_ss_, model_zero_curvature_keypoints, model_zero_curvature_keyNormals);
	std::cout << "Model total points: " << model->size() << std::endl;
	std::cout << "No zero total points: " << pnts_no_zero->size() << "; Selected downsample: " << model_no_zero_curvatur_keypoints->size() << std::endl;
	std::cout << "zero total points: " << pnts_zero->size() << "; Selected downsample: " << model_zero_curvature_keypoints->size() << std::endl;



	////
	//// change the center of the model
	////

	//pcl::PointCloud<PointType>::Ptr model_keypoints_changed(new pcl::PointCloud<PointType>());
	//model_keypoints_changed->reserve(model_keypoints->size());
	//for(size_t i=0;i<model_keypoints->size();++i)
	//{
		//PointType tmp;
		//tmp.x=model_keypoints->at(i).x-model_approximate_center(0);
		//tmp.y=model_keypoints->at(i).y-model_approximate_center(1);
		//tmp.z=model_keypoints->at(i).z-model_approximate_center(2);
		//model_keypoints_changed->push_back(tmp);
	//}

		if(save_sampled_cloud_)
		{
			pcl::io::savePLYFile(model_filename_+"_changed",*model);
		}
	//
	//visualize keypoints
	//

	pcl::visualization::PCLVisualizer key_visual("keyPoint detect");
	key_visual.addCoordinateSystem(20);
	key_visual.addPointCloudNormals<PointType, NormalType>(model_no_zero_curvatur_keypoints, model_no_zero_curvature_keyNormals, 1, 10, "model_no_zero_normals");
	key_visual.addPointCloud(model_no_zero_curvatur_keypoints, pcl::visualization::PointCloudColorHandlerCustom<PointType>(model_no_zero_curvatur_keypoints, 0.0, 0.0, 255.0), "model_no_zero_keypoints");
	key_visual.addPointCloudNormals<PointType, NormalType>(model_zero_curvature_keypoints, model_zero_curvature_keyNormals, 1, 10, "model_zero_normals");
	key_visual.addPointCloud(model_zero_curvature_keypoints, pcl::visualization::PointCloudColorHandlerCustom<PointType>(model_zero_curvature_keypoints, 0.0, 255.0, 0.0), "model_zero_keypoints");
	key_visual.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 4, "model_no_zero_keypoints");
	key_visual.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 4, "model_zero_keypoints");
	key_visual.spin();

	//
	// combine key pnts and key normals
	//
	pcl::PointCloud<PointType>::Ptr keypoints(new pcl::PointCloud<PointType>());
	pcl::PointCloud<NormalType>::Ptr keyNormals(new pcl::PointCloud<NormalType>());
	keypoints = model_no_zero_curvatur_keypoints;
	keyNormals = model_no_zero_curvature_keyNormals;
	if (plane_ss_ > 0){
		*keypoints += *model_zero_curvature_keypoints;
		*keyNormals += *model_zero_curvature_keyNormals;
	}
	//
	//  Compute Model Descriptors  PPF Space for model 
	//
	std::vector<double>model_size;
	model_size.push_back(model_width);
	model_size.push_back(model_length);
	model_size.push_back(model_height);
	std::sort(model_size.begin(), model_size.end());
	cout << "model_size after sort :";
	for (int32_t i = 0; i < 3; i++)
		cout << model_size[i] << " \t";
	cout << endl;

	//model ppf space
	zyk::PPF_Space model_feature_space;
	model_feature_space.init(keypoints, keyNormals, 0.15, float(model_ss_),true);
	model_feature_space.model_size[0]=model_size[0];
	model_feature_space.model_size[1]=model_size[1];
	model_feature_space.model_size[2]=model_size[2];
	model_feature_space.model_res = model_ss_;
	//
	// compute no empty ppf box nunber
	int32_t cnt = 0;
	for (int32_t i = 0; i < model_feature_space.getBoxVector()->size(); ++i)
	{
		if (model_feature_space.getBoxVector()->at(i) != NULL)
			cnt++;
	}
	cout << "no empty box number is: " << cnt << endl;
	model_feature_space.save(save_filename_);
	//CFile fileStore;
	//if (fileStore.Open(save_filename_.c_str(), CFile::modeWrite | CFile::modeCreate))
	//{
	//	CArchive arStore(&fileStore, CArchive::store);
	//	model_feature_space.Serialize(arStore);
	//	arStore.Close();
	//	fileStore.Close();
	//}
	

	return 0;
}
