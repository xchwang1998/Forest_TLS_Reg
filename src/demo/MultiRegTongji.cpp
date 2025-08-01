#include "math.h"

#ifndef _HLP_H_Included_
#define _HLP_H_Included_
#include "../include/utils/Hlp.h"
#endif

#ifndef _DST_H_Included_
#define _DST_H_Included_
#include "../include/dst/DST.h"
#endif

#ifndef _GTSAMOPTI_H_Included_
#define _GTSAMOPTI_H_Included_
#include "../include/gtsam_opti/gtsamOpti.h"
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
// #include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
// time
#include <chrono>

using namespace std;
using namespace cv;
using namespace pcl;
using namespace Eigen;
using namespace gtsam;


int main(int argc, char **argv) 
{
    // check the num of parameters
    if (argc < 2) {
        std::cout << "Error, at least 1 parameter" << std::endl;
        std::cout << "USAGE: ./MultiRegTongji [Plot NUM]" << std::endl;
        return 1;
    }
    
    // start time
    auto begin = std::chrono::steady_clock::now();
    
    std::string data_path = PROJECT_PATH;
    std::cout << BOLDGREEN << "----------------DATA PROCESSING----------------" << RESET << std::endl;
    // read the setting parameters
    ConfigSetting config_setting;
    ReadParas(data_path+"/config/tj_para.yaml", config_setting);   
    // std::cout << "here is good-1!" << std::endl; // debug
    std::vector<std::pair<Eigen::Vector3d, Eigen::Matrix3d>> effectivenessData;
    std::vector<std::pair<Eigen::Vector3d, Eigen::Matrix3d>> robustnessData;
    // read ground truth of transformation
    std::vector<std::tuple<int, int, Eigen::Matrix4d>> transformations;
    readTongjiTrans(data_path+"/data/tj/GroundTruthMatrices-Plot"+argv[1]+".txt", 
                    effectivenessData, robustnessData, transformations);
    std::cout << "effectivenessData: " << effectivenessData.size() << ", robustnessData: " << robustnessData.size() 
              << ", transformations: " << transformations.size() << std::endl;

    // read TLS data
    int StationNUM = effectivenessData.size() + 1;
    pcl::PointCloud<pcl::PointXYZ>::Ptr tls_point_data(new pcl::PointCloud<pcl::PointXYZ>);
    std::vector<std::pair<Eigen::Vector3d, Eigen::Matrix3d>> fine_pose;
    std::vector<std::pair<Eigen::Vector3d, Eigen::Vector3d>> rough_error, fine_error;
    HashRegDescManager *HashReg_RefTLS = new HashRegDescManager(config_setting);
    std::vector<FrameInfo> frameInfoVec;
    std::vector<pcl::PointCloud<pcl::PointXYZ>> ds_points;
    double gen_dt = 0, add_dt = 0;
    for(int i=1; i<=StationNUM; i++)
    {
        std::cout << BOLDGREEN << "--------------------Reading TLS Data-------------------" << RESET << std::endl;
        tls_point_data->clear();
        pcl::io::loadPLYFile(data_path+"/data/tj/S"+std::to_string(i)+".ply", *tls_point_data);
        std::cout << data_path+"/data/tj/S"+std::to_string(i)+".ply num:" << tls_point_data->size() << std::endl;

        std::cout << BOLDGREEN << "----------------Generate Descriptor----------------" << RESET << std::endl;
        // generate the tri descriptor and save (Target)
        FrameInfo reference_tls_info;
        auto t_1 = std::chrono::high_resolution_clock::now();
        HashReg_RefTLS->GenTriDescs(tls_point_data, reference_tls_info);
        auto t_2 = std::chrono::high_resolution_clock::now();
        HashReg_RefTLS->AddTriDescs(reference_tls_info);
        auto t_3 = std::chrono::high_resolution_clock::now();
        
        gen_dt += time_inc(t_2, t_1);
        add_dt += time_inc(t_3, t_2);
        frameInfoVec.push_back(reference_tls_info);

        std::cout << "FrameID: " << reference_tls_info.frame_id_
                << " triangles NUM: " << reference_tls_info.desc_.size() 
                << " feature points: "<< reference_tls_info.currCenter->points.size() << std::endl;

        // down sample and then store the points data
        down_sampling_voxel(*tls_point_data, 0.05);
        ds_points.push_back(*tls_point_data);
        // std::cout << "Downsampling (" + std::to_string(i) +"), Points NUM: " << tls_point_data->size() << std::endl;
    }
    gen_dt = gen_dt/StationNUM;
    add_dt = add_dt/StationNUM;
    std::cout << BOLDBLUE << "Average GenTriDescs_t: " << gen_dt << " Average AddTriDescs_t: " << add_dt << RESET << std::endl;

    // /************for display, the range image, stem and its position*************/
    // // range images
    // std::cout << "range images NUM: " << HashReg_RefTLS->pointMat_vec_.size() << std::endl;
    // for(int i=0; i<HashReg_RefTLS->pointMat_vec_.size(); i++)
    // {
    //     resizePixVal(HashReg_RefTLS->pointMat_vec_[i]); // resize the pixel value into [0-1]
    //     cv::imwrite(data_path+"/data/tj/" + std::to_string(i) + "-ori_fig.png", HashReg_RefTLS->pointMat_vec_[i]); 

    //     // save the filtered fig
    //     for(int j=0; j<HashReg_RefTLS->pointMat_vec_[i].rows; j++)
    //     {
    //         for(int k=0; k<HashReg_RefTLS->pointMat_vec_[i].cols; k++)
    //         {
    //             if(HashReg_RefTLS->pointTypeMat_vec_[i].at<float>(j, k) != 1)
    //             {
    //                 HashReg_RefTLS->pointMat_vec_[i].at<float>(j, k) = 0;
    //             }
    //         }
    //     }
    //     cv::imwrite(data_path+"/data/tj/" + std::to_string(i) + "-filterd_fig.png", HashReg_RefTLS->pointMat_vec_[i]); 
    // }

    // // stem position and its points cloud
    // std::cout << "clusters_vec_ NUM: " << HashReg_RefTLS->clusters_vec_.size() << std::endl;
    // for(int i=0; i<HashReg_RefTLS->clusters_vec_.size(); i++)
    // {
    //     pcl::PointCloud<pcl::PointXYZINormal>::Ptr stem_position(new pcl::PointCloud<pcl::PointXYZINormal>);
    //     pcl::PointCloud<pcl::PointXYZRGB>::Ptr objs(new pcl::PointCloud<pcl::PointXYZRGB>);
        
    //     for(int j=0; j<HashReg_RefTLS->clusters_vec_[i].size(); j++)
    //     {
    //         // get the stem position
    //         pcl::PointXYZINormal p = HashReg_RefTLS->clusters_vec_[i][j].p_center_;    
    //         stem_position->push_back(p);
            
    //         // get the stem points with different colors
    //         pcl::PointXYZRGB pk;
    //         double r = rand()/256; double g = rand()/256; double b = rand()/256;
    //         for(int k=0; k<HashReg_RefTLS->clusters_vec_[i][j].points_.size(); k++)
    //         {
    //             pk.x = HashReg_RefTLS->clusters_vec_[i][j].points_[k].x;
    //             pk.y = HashReg_RefTLS->clusters_vec_[i][j].points_[k].y;
    //             pk.z = HashReg_RefTLS->clusters_vec_[i][j].points_[k].z;

    //             pk.r = r; pk.g = g; pk.b = b;

    //             objs->push_back(pk);
    //         }
    //     }
    //     std::cout << "stem_position NUM: " << stem_position->size() << std::endl;
    //     pcl::io::savePCDFile(data_path+"/data/tj/" + std::to_string(i) + "-stem_position.pcd", *stem_position);
    //     pcl::io::savePCDFile(data_path+"/data/tj/" + std::to_string(i) + "-stems.pcd", *objs);
    // }
    
    // // discarded clusters
    // std::cout << "discarded clusters NUM: " << HashReg_RefTLS->discarded_clusters_vec.size() << std::endl;
    // for(int i=0; i<HashReg_RefTLS->discarded_clusters_vec.size(); i++)
    // {
    //     pcl::PointCloud<pcl::PointXYZRGB>::Ptr objs(new pcl::PointCloud<pcl::PointXYZRGB>);
    //     for(int j=0; j<HashReg_RefTLS->discarded_clusters_vec[i].size(); j++)
    //     {
    //         // get the stem points with different colors
    //         pcl::PointXYZRGB pk;
    //         double r = rand()/256; double g = rand()/256; double b = rand()/256;
    //         for(int k=0; k<HashReg_RefTLS->discarded_clusters_vec[i][j].points_.size(); k++)
    //         {
    //             pk.x = HashReg_RefTLS->discarded_clusters_vec[i][j].points_[k].x;
    //             pk.y = HashReg_RefTLS->discarded_clusters_vec[i][j].points_[k].y;
    //             pk.z = HashReg_RefTLS->discarded_clusters_vec[i][j].points_[k].z;

    //             pk.r = r; pk.g = g; pk.b = b;

    //             objs->push_back(pk);
    //         }
    //     }
    //     std::cout << "current station discard_cluster NUM: " << HashReg_RefTLS->discarded_clusters_vec[i].size() << std::endl;
    //     pcl::io::savePCDFile(data_path+"/data/tj/" + std::to_string(i) + "-discard_cluster.pcd", *objs);
    // }
    // /************for display the stem and its position*************/

    std::vector<CandidateInfo> candidates_vec;
    std::vector<TLSPos> tlsVec;
    double search_dt = 0;
    for(int i=0; i<frameInfoVec.size(); i++)
    {
        std::cout << BOLDGREEN << "----------------Searching----------------" << RESET << std::endl;
        std::cout << BOLDGREEN << "Frame ID: " << frameInfoVec[i].frame_id_ << RESET << std::endl;
        auto t_query_begin = std::chrono::high_resolution_clock::now();
        std::vector<std::pair<int, double>> search_result;
        std::vector<std::pair<Eigen::Vector3d, Eigen::Matrix3d>> loop_transform;
        std::vector<std::vector<std::pair<TriDesc, TriDesc>>> loop_triangle_pair;
        FrameInfo searchMap = frameInfoVec[i];
        HashReg_RefTLS->SearchMultiPosition(searchMap, search_result, loop_transform, loop_triangle_pair);
        auto t_query_end = std::chrono::high_resolution_clock::now();
        std::cout << "[Time] query: " << time_inc(t_query_end, t_query_begin) << "ms" << std::endl;
        search_dt += time_inc(t_query_end, t_query_begin);

        std::cout << "Search_result: " << search_result.size() << std::endl;
        CandidateInfo currt_candidate;
        currt_candidate.currFrameID = i;
        for(int j=0; j<search_result.size(); j++)
        {
            if(search_result[j].first != -1 && search_result[j].second > config_setting.icp_threshold)
            {
                std::cout << "candidate ID: " << search_result[j].first << " score: " << search_result[j].second << std::endl;
                currt_candidate.candidateIDScore.push_back(search_result[j]);
                currt_candidate.relativePose.push_back(loop_transform[j]);
                currt_candidate.triMatch.push_back(loop_triangle_pair[j]);
                
                // std::cout << "loop_transform " << std::to_string(i) << ", " << std::to_string(search_result[j].first) << std::endl;
                // std::cout << loop_transform[j].first << std::endl;
            }
        }

        candidates_vec.push_back(currt_candidate);
        
        TLSPos tmpPos;
        tmpPos.ID = i;
        tmpPos.isValued = false;
        tlsVec.push_back(tmpPos);
    }
    search_dt = search_dt/frameInfoVec.size();
    std::cout << BOLDBLUE << "Average Search_dt: " << search_dt << RESET << std::endl;
    
    // calculated the initial pos of each TLS station
    RelaToAbs(candidates_vec, tlsVec);
    
    // // TLSPos initNode;
    // // initNode.ID = 0;
    // // initNode.R = Eigen::Matrix3d::Identity();
    // // initNode.t = Eigen::Vector3d::Zero();
    // // initNode.isValued = true;
    // // std::unordered_set<int> visited;
    // // AbsByDFS(0, initNode, candidates_vec, tlsVec, visited);
    
    //  calculated the ground truth of each TLS station
    std::vector<Eigen::Affine3d> tlsTrans;
    for(int i=0; i<StationNUM; i++)
    {
        Eigen::Affine3d currPos;
        if(i==0)
        {
            currPos = Eigen::Affine3d::Identity();
            tlsTrans.push_back(currPos);
        }
        else
        {
            currPos =  Eigen::Translation3d(effectivenessData[i-1].first) * effectivenessData[i-1].second;
            tlsTrans.push_back(currPos);
        }
    }
    // calculated the error of each station
    std::vector<PosError> rough_errors;
    accur_evaluation_vec(tlsVec, tlsTrans, rough_errors);
    write_error_vec(data_path+"/data/tj/roughError.txt", rough_errors);
    write_pose(data_path+"/data/tj/roughPoses.txt", tlsVec);

    // GTSAM optimization
    std::cout << BOLDGREEN << "----------------GTSAM Optimization----------------" << RESET << std::endl;
    double gtsam_dt1 = 0, gtsam_dt2 = 0;
    auto t_4 = std::chrono::high_resolution_clock::now();
    std::pair<double, double> var;
    var.first = 1e-4; var.second = 1e-2;
    gtsam::Values result;
    GTSAMOptimization(tlsVec, candidates_vec, result, var);

    // get the optimized transform information by GTSAM 
    std::vector<TLSPos> optiTLSVec;
    GTSAMResToPose(result, optiTLSVec);
    auto t_5 = std::chrono::high_resolution_clock::now();
    gtsam_dt1 = time_inc(t_5, t_4);
    std::cout << BOLDBLUE << "Average gtsam_dt1: " << gtsam_dt1 << " ms" << RESET << std::endl;
    
    // GTSAM accuracy evoluation
    std::vector<PosError> gtsam_errors;
    accur_evaluation_vec(optiTLSVec, tlsTrans, gtsam_errors);
    write_error_vec(data_path+"/data/tj/GTSAMError.txt", gtsam_errors);
    write_pose(data_path+"/data/tj/GTSAMPoses.txt", optiTLSVec);

    // Fine registration Using ICP
    std::cout << BOLDGREEN << "----------------ICP Fine Registration----------------" << RESET << std::endl;
    double icp_dt = 0, regNum = 0;
    // ICP Optimization
    for(int i=0; i<candidates_vec.size(); i++)
    {
        // get the currentID and target station
        pcl::PointCloud<pcl::PointXYZ>::Ptr ICP_target(new pcl::PointCloud<pcl::PointXYZ>);
        int currID = candidates_vec[i].currFrameID;
        // // from ply file
        // pcl::io::loadPLYFile(data_path+"/data/tj/S"+std::to_string(currID+1)+".ply", *ICP_target);
        // std::cout << data_path+"/data/tj/S"+std::to_string(currID+1)+".ply : " << ICP_target->size() << std::endl;

        // from downsampled data
        *ICP_target = ds_points[currID];

        std::pair<Eigen::Vector3d, Eigen::Matrix3d> curr_transform, cand_transform;
        // get the current transform
        if(optiTLSVec[currID].isValued)
        {
            // init trans of target
            curr_transform.first = optiTLSVec[currID].t;
            curr_transform.second = optiTLSVec[currID].R;

            curr_transform = matrixInv(curr_transform);
            // trans_point_cloud(curr_transform, ICP_target);
            // writeLas(data_path+"/data/tj/"+std::to_string(currID+1) +"-rough.las", ICP_target);
            // down_sampling_voxel(*ICP_target, 0.03);
        }
        
        // update the candidate scan
        for(int j=0; j<candidates_vec[i].candidateIDScore.size(); j++)
        {
            // get the candidate ID and the scan
            int candID = candidates_vec[i].candidateIDScore[j].first;
            // read the candidate las data
            pcl::PointCloud<pcl::PointXYZ>::Ptr ICP_source(new pcl::PointCloud<pcl::PointXYZ>);
            
            // pcl::io::loadPLYFile(data_path+"/data/tj/S"+std::to_string(candID+1)+".ply", *ICP_source);
            // std::cout << data_path+"/data/tj/S"+std::to_string(candID+1)+".ply : " << ICP_source->size() << std::endl;
            
            // get the scan data
            *ICP_source = ds_points[candID];
            
            if(optiTLSVec[candID].isValued)
            {
                // init trans of source
                cand_transform.first = optiTLSVec[candID].t;
                cand_transform.second = optiTLSVec[candID].R;
                // trans_point_cloud(cand_transform, ICP_source);
                // writeLas(data_path+"/data/tj/"+std::to_string(candID+1) +"-rough.las", ICP_source);
                
                // calculate the transformation between target and source and trans to the target
                std::pair<Eigen::Vector3d, Eigen::Matrix3d> poseBTW;
                poseBTW = matrixMultip(curr_transform, cand_transform);
                trans_point_cloud(poseBTW, ICP_source);
                // writeLas(data_path+"/data/tj/"+std::to_string(candID+1) +"-rough.las", ICP_source);

                // down_sampling_voxel(*ICP_source, 0.05);
                // down_sampling_voxel(*ICP_target, 0.05);
                
                // std::cout << "here is good-1" << std::endl;
                // fine registration by gicp
                auto t_update_gicp_begin = std::chrono::high_resolution_clock::now();
                std::pair<Eigen::Vector3d, Eigen::Matrix3d> refine_transform_gicp;
                // fast_gicp_registration(ICP_source, ICP_target, refine_transform_gicp);
                small_gicp_registration(ICP_source, ICP_target, refine_transform_gicp);
                // pcl_gicp_registration(ICP_source, ICP_target, refine_transform_gicp);
                auto t_update_gicp_end = std::chrono::high_resolution_clock::now();
                std::cout << "FINE: target: " << std::to_string(currID+1) << " source: " << std::to_string(candID+1) << std::endl;
                std::cout << "[Time] GICP Reg: " << time_inc(t_update_gicp_end, t_update_gicp_begin) << "ms" << std::endl;  
                
                icp_dt += time_inc(t_update_gicp_end, t_update_gicp_begin);               
                regNum++;

                // get the ICP fine registration
                std::pair<Eigen::Vector3d, Eigen::Matrix3d> fine_trans;
                fine_trans = matrixMultip(refine_transform_gicp, poseBTW);
                
                // Debug, save the re-fined point cloud to have a test
                // trans_point_cloud(refine_transform_gicp, ICP_source);
                // writeLas(data_path+"/data/tj/"+std::to_string(candID+1) +"-fine.las", ICP_source);
                
                // std::cout << fine_trans.first << std::endl;
                // std::cout << fine_trans.second << std::endl;
                // candidates_vec[i].relativePose[j] = matrixInv(candidates_vec[i].relativePose[j]);
                // std::cout << "Ori Trans: " << std::endl;
                // std::cout << candidates_vec[i].relativePose[j].first << std::endl;
                // std::cout << candidates_vec[i].relativePose[j].second << std::endl;
                
                // update the relative pose
                candidates_vec[i].relativePose[j] = matrixInv(fine_trans);
            }   
        }
    }

    icp_dt = icp_dt/regNum;
    std::cout << BOLDBLUE << "Average ICP_dt: " << icp_dt << " ms, " << "regNum: " << regNum << RESET << std::endl;

    std::vector<TLSPos> optiPosVec;
    optiPosVec.resize(tlsVec.size());
    
    // calculated the ICP optimized pos of each TLS station
    RelaToAbs(candidates_vec, optiPosVec);
    std::vector<PosError> ICP_errors;
    accur_evaluation_vec(optiPosVec, tlsTrans, ICP_errors);
    write_error_vec(data_path+"/data/tj/icpError.txt", ICP_errors);
    write_pose(data_path+"/data/tj/icpPoses.txt", optiPosVec);

    std::cout << BOLDGREEN << "----------------GTSAM Optimization Again----------------" << RESET << std::endl;
    auto t_6 = std::chrono::high_resolution_clock::now();
    var.first = 1e-6; var.second = 1e-4;
    gtsam::Values finalResult;
    GTSAMOptimization(optiPosVec, candidates_vec, finalResult, var);
    // get the optimized transform information by GTSAM 
    std::vector<TLSPos> finalTLSVec;
    GTSAMResToPose(finalResult, finalTLSVec);
    auto t_7 = std::chrono::high_resolution_clock::now();
    
    gtsam_dt2 = time_inc(t_7, t_6);
    std::cout << BOLDBLUE << "Average gtsam_dt2: " << gtsam_dt2 << " ms" << RESET << std::endl;
     
    std::vector<PosError> Final_errors;
    accur_evaluation_vec(finalTLSVec, tlsTrans, Final_errors);
    write_error_vec(data_path+"/data/tj/finalError.txt", Final_errors);
    write_pose(data_path+"/data/tj/finalPoses.txt", finalTLSVec);

    // write the running time for each module
    std::ofstream outfile(data_path+"/data/tj/time_stastic.txt");
    outfile << "Average GenTriDescs_t: " << gen_dt << std::endl;
    outfile << "Average AddTriDescs_t: " << add_dt << std::endl;
    outfile << "Average Search_dt: " << search_dt << std::endl;
    outfile << "Average gtsam_dt1: " << gtsam_dt1 << std::endl;
    outfile << "Average ICP_dt: " << icp_dt << std::endl;
    outfile << "ICP Reg Num: " << regNum << std::endl;
    outfile << "Average gtsam_dt2: " << gtsam_dt2 << std::endl;
    outfile.close();

    std::cout << BOLDGREEN << "----------------Finish!----------------" << RESET << std::endl;

    // end time
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> past = std::chrono::duration_cast<std::chrono::duration<double>>(end - begin);
    std::cout << BOLDGREEN << "total cost time:" << past.count() << " sec\n" << RESET;
    std::ofstream timeFile(data_path+"/data/tj/run_time.txt");
    timeFile << past.count();
    timeFile.close();

    return 0;
}
