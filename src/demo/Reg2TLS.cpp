#include "math.h"

#ifndef _HLP_H_Included_
#define _HLP_H_Included_
#include "../include/utils/Hlp.h"
#endif

#ifndef _DST_H_Included_
#define _DST_H_Included_
#include "../include/dst/DST.h"
#endif

#ifndef _GENALG_H_Included_
#define _GENALG_H_Included_
#include "../include/utils/GenAlg.h"
#endif

#ifndef _POINT2IMG_H_Included_
#define _POINT2IMG_H_Included_
#include "../include/utils/Point2img.h"
#endif

#ifndef _FEC_H_Included_
#define _FEC_H_Included_
#include "../include/utils/FEC.h"
#endif

#ifndef _HashRegObj_H_Included_
#define _HashRegObj_H_Included_
#include "../include/utils/HashRegObj.h"
#endif

// C++
#include <iostream>
// pcl
#include <pcl/io/pcd_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

using namespace std;
using namespace cv;
using namespace pcl;
using namespace Eigen;

int main(int argc, char **argv) 
{
    // check the num of parameters
    if (argc < 2) {
        std::cout << RED << "Error, at least 2 parameter" << RESET << std::endl;
        std::cout << "USAGE: ./Reg2TLSPoints [Target Station's File Name] [Source Station's File Name]" << std::endl;
        return 1;
    }

    std::string data_path = PROJECT_PATH;
    std::cout << BOLDGREEN << "----------------DATA PROCESSING----------------" << RESET << std::endl;
    std::cout << BOLDGREEN << "--------------------Reading-------------------" << RESET << std::endl;
    std::cout << BOLDGREEN << data_path+"/config/snj_para.yaml" << RESET << std::endl;
    // read the setting parameters
    ConfigSetting config_setting;
    ReadParas(data_path+"/config/snj_para.yaml", config_setting);

    // read the grund truth of pose data
    std::vector<Eigen::Affine3d> tlsTrans = readTLSTrans(data_path+"/data/snj/transformation.txt");
    Eigen::Affine3d gt_pose = tlsTrans[std::atoi(argv[1])-1].inverse() * tlsTrans[std::atoi(argv[2])-1];
    std::cout << BOLDGREEN << "----------------Read Points Data----------------" << RESET << std::endl;
    std::cout << tlsTrans.size() << " TLS stations in this plot." << std::endl;
    // read TLS data
    pcl::PointCloud<pcl::PointXYZ>::Ptr reference_data(new pcl::PointCloud<pcl::PointXYZ>);
    readTLSData(data_path+"/data/snj/"+ argv[1]+".las", reference_data);
    pcl::PointCloud<pcl::PointXYZ>::Ptr source_data(new pcl::PointCloud<pcl::PointXYZ>);
    readTLSData(data_path+"/data/snj/"+ argv[2]+".las", source_data);
    std::cout << "Read Target Points NUM: " << reference_data->size() << std::endl;
    std::cout << "Read Source Points NUM: " << source_data->size() << std::endl;
    
    std::cout << BOLDGREEN << "----------------Generate Descriptor----------------" << RESET << std::endl;
    HashRegDescManager *HashReg_RefTLS = new HashRegDescManager(config_setting);
    // generate the tri descriptor and save (Target)
    FrameInfo reference_tls_info;
    HashReg_RefTLS->GenTriDescs(reference_data, reference_tls_info);
    writeLas(data_path+"/data/snj/"+ argv[1]+"-obj.las", reference_tls_info.currPoints);
    pcl::io::savePCDFileASCII(data_path+"/data/snj/"+ argv[1]+"-obj_center.pcd", *reference_tls_info.currCenter);
    HashReg_RefTLS->AddTriDescs(reference_tls_info);
    
    // generate the tri descriptor (Source)
    FrameInfo source_tls_info;
    HashReg_RefTLS->GenTriDescs(source_data, source_tls_info);
    writeLas(data_path+"/data/snj/"+ argv[2]+"-obj.las", source_tls_info.currPoints);
    pcl::io::savePCDFileASCII(data_path+"/data/snj/"+ argv[2]+"-obj_center.pcd", *source_tls_info.currCenter);
    
    std::cout << BOLDGREEN << "----------------Searching Triangle Pairs----------------" << RESET << std::endl;
    auto t_query_begin = std::chrono::high_resolution_clock::now();
    std::pair<int, double> search_result(-1, 0);
    std::pair<Eigen::Vector3d, Eigen::Matrix3d> loop_transform;
    loop_transform.first << 0, 0, 0;
    loop_transform.second = Eigen::Matrix3d::Identity();
    std::vector<std::pair<TriDesc, TriDesc>> loop_triangle_pair;
    HashReg_RefTLS->SearchPosition(source_tls_info, search_result, loop_transform, loop_triangle_pair);
    auto t_query_end = std::chrono::high_resolution_clock::now();
    std::cout << "[Time] query: " << time_inc(t_query_end, t_query_begin) << "ms" << std::endl;

    // std::cout << "search_result: " << search_result.first << ", " << search_result.second << std::endl;
    // std::cout << "loop_transform.first: \n" << loop_transform.first << std::endl;
    // std::cout << "loop_transform.second: \n" << loop_transform.second << std::endl;

    // std::cout << "FrameID: " << reference_tls_info.frame_id_
    //         << " triangles NUM: " << reference_tls_info.desc_.size() 
    //         << " feature points: "<< reference_tls_info.currCenter->points.size() << std::endl;    
    
    // find the corresponding TLS
    if(search_result.first != -1)
    {
        // accurcy evaluation
        std::pair<Eigen::Vector3d, Eigen::Vector3d> rough_error;
        accur_evaluation(loop_transform, gt_pose, rough_error);
        write_error(data_path+"/data/snj/"+ argv[2]+"-rough-error.txt", rough_error);
        
        // trans the source data, and save (rough trans data)
        trans_point_cloud(loop_transform, source_data);
        // writeLas(data_path+"/data/snj/"+ argv[2]+"-rough_trans.las", source_data);
        
        // trans obj points, and save (rough trans data)
        trans_point_cloud(loop_transform, source_tls_info.currPoints);
        // writeLas(data_path+"/data/snj/"+ argv[2]+"-obj-transed.las", source_tls_info.currPoints);
        
        // down_sampling_voxel(*reference_data, 0.03);
        std::cout << BOLDGREEN << "----------------Fine Registrating----------------" << RESET << std::endl;
        // fine registration by GICP
        auto t_update_gicp_begin = std::chrono::high_resolution_clock::now();
        std::pair<Eigen::Vector3d, Eigen::Matrix3d> refine_transform_gicp;
        small_gicp_registration(source_data, reference_data, refine_transform_gicp);
        // pcl_gicp_registration(source_data, reference_data, refine_transform_gicp);
        auto t_update_gicp_end = std::chrono::high_resolution_clock::now();
        std::cout << "[Time] Small_GICP: " << time_inc(t_update_gicp_end, t_update_gicp_begin) << "ms" << std::endl;  

        // finely trans the source data, and save
        trans_point_cloud(refine_transform_gicp, source_data);
        writeLas(data_path+"/data/snj/"+ argv[2]+"-fine_trans.las", source_data);

        // get the fine registration
        std::pair<Eigen::Vector3d, Eigen::Matrix3d> fine_trans;
        fine_trans.second = refine_transform_gicp.second * loop_transform.second;
        fine_trans.first = refine_transform_gicp.second * loop_transform.first + refine_transform_gicp.first;
        
        // accurcy evaluation (fine)
        std::pair<Eigen::Vector3d, Eigen::Vector3d> fine_error;
        accur_evaluation(fine_trans, gt_pose, fine_error);
        write_error(data_path+"/data/snj/"+ argv[2]+"-fine-error.txt", fine_error);
    }
    else
    {
        std::cout << BOLDRED << "----------------Failed Registration, Check Parameters----------------" << RESET << std::endl;
    }

    std::cout << BOLDGREEN << "----------------Finish!----------------" << RESET << std::endl;
    return 0;
}
