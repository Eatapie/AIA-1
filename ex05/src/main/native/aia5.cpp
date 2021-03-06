//============================================================================
// Name        : aia5.cpp
// Author      : Ronny Haensch
// Version     : 1.0
// Copyright   : -
// Description : Creates data points and estimates their distribution
//============================================================================

#define _USE_MATH_DEFINES
#include <cmath>

#include <iostream>
#include <stdio.h>
#include <sstream>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

// function headers
// functions to be written (see below)
Mat calcCompLogL(vector<struct comp*>& model, Mat& features);
Mat calcMixtureLogL(vector<struct comp*>& model, Mat& features);
Mat gmmEStep(vector<struct comp*>& model, Mat& features);
void gmmMStep(vector<struct comp*>& model, Mat& features, Mat& posterior);
// given functions (see below)
void initNewComponent(vector<struct comp*>& model, Mat& features);
void plotGMM(vector<struct comp*>& model, Mat& features);
void trainGMM(Mat& data, int numberOfComponents);

struct comp {Mat mean; Mat covar; double weight;};

// main function, generates random vectors and estimates their distribution
int main(int argc, char** argv) {

	// dimensionality of the generated feature vectors
	int vectLen = 2;

	// number of components (clusters) to generate
	int actualComponentNum = 6;

	// maximal standard deviation of vectors in each cluster
	double actualDevMax = 0.3;

	// number of vectors in each cluster
	int trainingSize = 150;

	// initialise random component parameters (mean and standard deviation)
	Mat actualMean = Mat(actualComponentNum, vectLen, CV_32FC1);
	Mat actualSDev = Mat(actualComponentNum, vectLen, CV_32FC1);
	randu(actualMean, 0, 1);
	randu(actualSDev, 0, actualDevMax);

	// print distribution parameters to screen
	cout << "true mean" << endl;
	cout << actualMean << endl;
	cout << "true sdev" << endl;
	cout << actualSDev << endl;

	// initialise random cluster vectors
	Mat trainingData = Mat::zeros(vectLen, trainingSize*actualComponentNum, CV_32FC1);
	int n=0;
	RNG rng;
	for (int c=0; c<actualComponentNum; c++) {
		for (int s=0; s<trainingSize; s++) {
			for (int d=0; d<vectLen; d++) {
				trainingData.at<float>(d,n) = rng.gaussian( actualSDev.at<float>(c, d) ) + actualMean.at<float>(c, d);
			}
			n++;
		}
	}

	// train the corresponding mixture model using EM...
	trainGMM(trainingData, actualComponentNum);

	return 0;
}

/**
 * Computes the log-likelihood of each feature vector
 * in every component of the supplied mixture model.
 *
 * @param model  structure containing model parameters
 * @param features matrix of feature vectors
 * @return the log-likelihood log(p(x_i|y_i=j))
 */
Mat calcCompLogL(vector<struct comp*>& model, Mat& features) {

	//cout << "calcCompLogL" << endl;
	int numFeatures = features.cols;
	const int& d = features.rows; // dimensions
	int numComponents = model.size();
	Mat compLogL(numFeatures, numComponents, CV_32FC1);

	for (int i = 0; i < numFeatures; ++i) {
		for (int j = 0; j < numComponents; ++j) {
			const Mat& mu_j = model.at(j)->mean;
			const Mat& sigma_j = model.at(j)->covar;

			const Mat& xCentered_j = features.col(i) - mu_j;
			compLogL.at<float>(i, j) =
					- 0.5f * log(determinant(sigma_j))
					- (d / 2.0f) * log(2 * M_PI)
					- 0.5f * ((Mat)(xCentered_j.t() * sigma_j.inv() * xCentered_j)).at<float>(0, 0);
		}
	}
	/*
	cout << "Rows (compLogL): " << compLogL.rows << endl;
	cout << "Cols (compLogL): " << compLogL.cols << endl;
	*/

	return compLogL;
	// TODO
}

/**
 * Computes the log-likelihood of each feature by combining the likelihoods
 * in all model components.
 *
 * @see aia5_WS12_em.pdf page 9 & 10
 *
 * @param model  structure containing model parameters
 * @param features  matrix of feature vectors
 * @return  the log-likelihood of feature number i in the mixture model
 *   (the log of the summation of alpha_j p(x_i|y_i=j) over j)
 *   p(X|Omega)
 */
Mat calcMixtureLogL(vector<struct comp*>& model, Mat& features) {

	/*
	cout << "calcMixtureLogL" << endl;
	cout << "model.size(): " << model.size() << endl;
	cout << "model.0.mean: " << model.at(0)->mean.rows << " " << model.at(0)->mean.cols << ": " << model.at(0)->mean << endl;
	cout << "model.0.covar: " << model.at(0)->covar.rows << " " << model.at(0)->covar.cols << ": " << model.at(0)->covar  << endl;
	cout << "model.0.weight: " << model.at(0)->weight << endl;
	cout << "features: " << features.rows << " " << features.cols << endl;
    */

	int numFeatures = features.cols;
	int numComponents = model.size();


	// getting all log likelihoods for each feature according to each single component
	//Mat compLogL(numFeatures, numComponents, CV_32FC1);
	Mat compLogL = calcCompLogL(model, features);


	//Mat logGaussianMixtureModel(model.size(), features.cols, features.type());
	Mat logGaussianMixtureModel(numFeatures, 1, features.type());


	for (int i = 0; i < features.cols; ++i) {

		// getting the log_c-scale-factor
		double log_c;
		double alpha_j = model.at(0)->weight;
		double log_alpha_j = log(alpha_j);
		double log_prob_j  = compLogL.at<float>(i, 0);
		log_c = log_alpha_j + log_prob_j;
		for (int j = 0; j < model.size(); ++j) {
			alpha_j = model.at(j)->weight;
			log_alpha_j = log(alpha_j);
			log_prob_j  = compLogL.at<float>(i,j);
			if (log_c < (log_alpha_j + log_prob_j)) {
				log_c = log_alpha_j + log_prob_j;
			}
		}
		double inner_log_sum = 0;
		for (int j = 0; j < model.size(); ++j) {
			alpha_j = model.at(j)->weight;
			log_alpha_j = log(alpha_j);
			log_prob_j = compLogL.at<float>(i,j);
			inner_log_sum += exp(log_alpha_j + log_prob_j - log_c);

			/*const Mat& mu_j = model.at(j)->mean;
			const Mat& sigma_j = model.at(j)->covar;
			const int& d = features.rows; // dimensions

			const Mat& xCentered_j = features.col(i) - mu_j;
			logGaussianMixtureModel.at<float>(i, j) = alpha_j *
					(log(pow(determinant(sigma_j), -0.5) / pow(2 * M_PI, d / 2.0))
					+ log(- 0.5f * ((Mat)(xCentered_j.t() * sigma_j.inv() * xCentered_j)).at<float>(0, 0)));*/
		}
		logGaussianMixtureModel.at<float>(i, 1) = log_c + log(inner_log_sum);
		//cout << "likelihood feature " << i << ": " << exp(logGaussianMixtureModel.at<float>(i, 1)) << endl;
	}
	// TODO

	return logGaussianMixtureModel;
}

/**
 * Computes the posterior over components
 * (the degree of component membership) for each feature.
 *
 * @param model  structure containing model parameters
 * @param features  matrix of feature vectors
 * @return  the posterior p(y_i=j|x_i)
 */
Mat gmmEStep(vector<struct comp*>& model, Mat& features) {

	//cout << "gmmEStep" << endl;
	int numFeatures = features.cols;
	int numComponents = model.size();
	Mat posterior(numFeatures, numComponents, CV_32FC1);
	double alpha_j;
	double log_alpha_j;
	double prob_feature_i;
	double prob_feature_i_componentwise;
	Mat mixLogL = calcMixtureLogL(model, features);
	Mat compLogL = calcCompLogL(model, features);
	for (int i = 0; i < numFeatures; i++){
		prob_feature_i = mixLogL.at<float>(i, 1);
		for (int j = 0; j < numComponents; j++) {
			alpha_j = model.at(j)->weight;
			log_alpha_j = log(alpha_j);
			prob_feature_i_componentwise = compLogL.at<float>(i, j);
			posterior.at<float>(i, j) = exp(log_alpha_j + prob_feature_i_componentwise - prob_feature_i);
		}
	}

	/*for (int j = 0; j < numComponents; j++) {
		cout << " gmmEStep mean : " << model.at(j)->mean << endl;
	}*/

	return posterior;
	// TODO
}

/**
 * Updates a given model on the basis of posteriors previously computed in the E-Step.
 *
 * @param model  structure containing model parameters, will be updated in-place
 *   new model structure in which all parameters have been updated to reflect
 *   the current posterior distributions.
 * @param features  matrix of feature vectors
 * @param posterior  the posterior p(y_i=j|x_i)
 */
void gmmMStep(vector<struct comp*>& model, Mat& features, Mat& posterior) {

	//cout << "gmmMStep" << endl;
	int numFeatures = features.cols;
	int numComponents = model.size();

	//Mat mixLogL = calcMixtureLogL(model, features);
	//Mat compLogL = calcCompLogL(model, features);

	//Mat mu_j;
	double N_j;
	double alpha_j;
	//Mat cov_j;
	for (int j = 0; j < numComponents; j++) {
		/*cout << "component : " << j << endl;
		cout << " old mean : " << model.at(j)->mean << endl;*/

		N_j = 0;
		Mat mu_j = Mat::zeros(features.rows, 1, CV_32FC1); // = model.at(j)->mean;
		Mat cov_j = Mat::zeros(features.rows, features.rows, CV_32FC1); //model.at(j)->covar;

		// more precise...
		for (int i = 0; i < numFeatures; i++) {
			N_j += posterior.at<float>(i, j);
			mu_j += features.col(i)*posterior.at<float>(i, j);
		}
		//cout << " new mean : " << mu_j << endl;

		// faster?
		/*N_j  = sum(posterior.col(j)).val[0];
		mu_j.at<float>(0) = sum(features.row(0)*posterior.col(j)).val[0];
		mu_j.at<float>(1) = sum(features.row(1)*posterior.col(j)).val[0];*/
		if (mu_j.at<float>(0) != 0) {
			mu_j = mu_j / N_j;
		}
		for (int i = 0; i < numFeatures; i++) {
			Mat xCentered_j = features.col(i) - mu_j;
			if (i == 0) {
				cov_j = ((Mat)(xCentered_j * xCentered_j.t())) * posterior.at<float>(i, j);
			} else {
				cov_j += ((Mat)(xCentered_j * xCentered_j.t())) * posterior.at<float>(i, j);
			}
		}
		if (cov_j.at<float>(0, 1) != 0) {
			cov_j = cov_j / N_j;
		}
		alpha_j = N_j / numFeatures;

		model.at(j)->weight = alpha_j;
		model.at(j)->mean = mu_j;
		model.at(j)->covar = cov_j;

		/*cout << " new mean : " << model.at(j)->mean << endl;
		cout << " weight : " << model.at(j)->weight << endl;

		cout << " all means : " << endl;
		for (int k= 0; k < numComponents; k++){
			cout << "     mean " << k << " : " << model.at(k)->mean << endl;
		}
		*/
	}
	// TODO
}

/* ***********************
   *** Given Functions ***
   *********************** */

// Trains a Gaussian mixture model with a specified number of components on the basis of a given set of feature vectors.
/*
data:     		feature vectors, one vector per column
numberOfComponents:	the desired number of components in the trained model
*/
void trainGMM(Mat& data, int numberOfComponents) {

	// the number and dimensionality of feature vectors
	int featureNum = data.cols;
	int featureDim = data.rows;

	// initialize the model with one component and arbitrary parameters
	struct comp fst;
	fst.weight = 1;
	fst.mean = Mat::zeros(featureDim, 1, CV_32FC1);
	fst.covar = Mat::eye(featureDim, featureDim, CV_32FC1);
	vector<struct comp*> model;
	model.push_back(&fst);

	plotGMM(model, data);

	// the data-log-likelihood
	double dataLogL[2] = {0,0};

	// iteratively add components to the mixture model
	for (int i=1; i<=numberOfComponents; i++) {
		cout << "Current number of components: " << i << endl;

		// the current combined data log-likelihood p(X|Omega)
		Mat mixLogL = calcMixtureLogL(model, data);
		dataLogL[0] = sum(mixLogL).val[0];
		dataLogL[1] = 0.;

		/*cout << "Rows (mixLogL): " << mixLogL.rows << endl;
		cout << "Cols (mixLogL): " << mixLogL.cols << endl;
		*/

		// EM iteration while p(X|Omega) increases
		int it = 0;
		while ( (dataLogL[0] > dataLogL[1]) || (it == 0) ) {

			it++;
			//printf("Iteration: %d\t: %f\n", it, dataLogL[0]);

			// E-Step (computes posterior)
			Mat posterior = gmmEStep(model, data);

			// M-Step (updates model parameters)
			gmmMStep(model, data, posterior);

			// update the current p(X|Omega)
			dataLogL[1] = dataLogL[0];
			mixLogL = calcMixtureLogL(model, data);
			dataLogL[0] = sum(mixLogL).val[0];
		}
		cout << endl;

		// visualize the current model (with i components trained)
		if (featureDim >= 2) {
			plotGMM(model, data);
		}

		// add a new component if necessary
		if (i < numberOfComponents) {
			initNewComponent(model, data);
		}
	}

	cout << endl << "**********************************" << endl;
	cout << "Trained model: " << endl;
	for (int i=0; i<model.size(); i++) {
		cout << "Component " << i << endl;
		cout << "\t>> weight: " << model.at(i)->weight << endl;
		cout << "\t>> mean: " << model.at(i)->mean << endl;
		cout << "\t>> std: [" << sqrt(model.at(i)->covar.at<float>(0,0)) << ", " << sqrt(model.at(i)->covar.at<float>(1,1)) << "]" << endl;
		cout << "\t>> covar: " << endl;
		cout << model.at(i)->covar << endl;
		cout << endl;
	}
	// wait a last time
	waitKey(0);
}

// Adds a new component to the input mixture model by spliting one of the existing components in two parts.
/*
model:		Gaussian Mixture Model parameters, will be updated in-place
features:	feature vectors
*/
void initNewComponent(vector<struct comp*>& model, Mat& features) {

	// number of components in current model
	int compNum = model.size();

	// number of features
	int featureNum = features.cols;

	// dimensionality of feature vectors (equals 3 in this exercise)
	int featureDim = features.rows;

	// the largest component is split (this is not a good criterion...)
	int splitComp = 0;
	for (int i=0; i<compNum; i++) {
		if (model.at(splitComp)->weight < model.at(i)->weight) {
			splitComp = i;
		}
	}

	// split component 'splitComp' along its major axis
	Mat eVec, eVal;
	eigen(model.at(splitComp)->covar, eVal, eVec);

	Mat devVec = 0.5 * sqrt( eVal.at<float>(0) ) * eVec.row(0).t();

	// create new model structure and compute new mean values, covariances, new component weights...
	struct comp* newModel = new struct comp;
	newModel->weight = 0.5 * model.at(splitComp)->weight;
	newModel->mean = model.at(splitComp)->mean - devVec;
	newModel->covar = 0.25 * model.at(splitComp)->covar;

	// modify the split component
	model.at(splitComp)->weight = 0.5*model.at(splitComp)->weight;
	model.at(splitComp)->mean += devVec;
	model.at(splitComp)->covar *= 0.25;

	// add it to old model
	model.push_back(newModel);
}

static int numClus = 0;

// Visualises the contents of a feature space and the associated mixture model.
/*
model: 		parameters of a Gaussian mixture model
features: 	feature vectors

Feature vectors are plotted as blue points
The means of components are indicated by red circles
The eigenvectors of the covariance are indicated by blue ellipses
If the feature space has more than 2 dimensions, only the first two dimensions are visualized.
*/
void plotGMM(vector<struct comp*>& model, Mat& features) {

	// size of the plot
	int imSize = 500;

	// get scaling factor to scale coordinates
	double max_x=0, max_y=0, min_x=0, min_y=0;
	for (int n=0; n<features.cols; n++) {
		if (max_x < features.at<float>(0, n) )
			max_x = features.at<float>(0, n);
		if (min_x > features.at<float>(0, n) )
			min_x = features.at<float>(0, n);
		if (max_y < features.at<float>(1, n) )
			max_y = features.at<float>(1, n);
		if (min_y > features.at<float>(1, n) )
			min_y = features.at<float>(1, n);
	}
	double scale = (imSize-1)/max((max_x - min_x), (max_y - min_y));
	// create plot
	Mat plot = Mat(imSize, imSize, CV_8UC3, Scalar(255,255,255) );

	// set feature points
	for (int n=0; n<features.cols; n++) {
		plot.at<Vec3b>( ( features.at<float>(0, n) - min_x ) * scale, max( (features.at<float>(1,n)-min_y)*scale, 5.) ) = Vec3b(255,0,0);
	}
	// get ellipse of components
	Mat EVec, EVal;
	for (int i=0; i<model.size(); i++) {

		eigen(model.at(i)->covar, EVal, EVec);
		double rAng = atan2(EVec.at<float>(0, 1), EVec.at<float>(0, 0));

		// draw components
		circle(plot,  Point( (model.at(i)->mean.at<float>(1,0)-min_y)*scale, (model.at(i)->mean.at<float>(0,0)-min_x)*scale ), 3, Scalar(0,0,255), 2);
		ellipse(plot, Point( (model.at(i)->mean.at<float>(1,0)-min_y)*scale, (model.at(i)->mean.at<float>(0,0)-min_x)*scale ), Size(EVal.at<float>(1)*scale*3, EVal.at<float>(0)*scale*3), rAng*180/CV_PI, 0, 360, Scalar(0,255,0), 1);
	}

	// show plot an abd wait for key
	imshow("Current model", plot);
	ostringstream ss;
	ss << numClus++ << "_Cluster.png";
	imwrite(ss.str(), plot);
	waitKey(0);
}
