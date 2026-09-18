#pragma once

struct P12DiagnosticWriteJob {
	std::uint64_t sourceSeq=0;
	std::string captureBase;
	std::string readbackRoute;
	std::string domain;
	std::string unit;
	int commonScaledLinear=0;
	int physicalRadiance=0;
	int width=0;
	int height=0;
	std::shared_ptr<PfmFile> linear;
	std::shared_ptr<PfmFile> stats;
	bool statsRequired=false;
	std::vector<unsigned char> rgb;
	std::chrono::steady_clock::time_point enqueued;
};

// Diagnostic-only bounded CPU writer. All OpenGL access remains on the render
// thread; this queue owns the copied RGB bytes and PfmFile objects it receives.
class P12DiagnosticWriterQueue {
public:
	static P12DiagnosticWriterQueue& instance(){static P12DiagnosticWriterQueue queue;return queue;}
	bool enqueue(P12DiagnosticWriteJob&& job,std::size_t& queueDepth){
		std::lock_guard<std::mutex> lock(m_mutex);
		if(m_stopping||m_jobs.size()>=kCapacity){queueDepth=m_jobs.size();return false;}
		job.enqueued=std::chrono::steady_clock::now();
		m_jobs.emplace_back(std::move(job));
		queueDepth=m_jobs.size();
		m_maxDepth=std::max(m_maxDepth,queueDepth);++m_submitted;
		m_ready.notify_one();return true;
	}
	std::size_t capacity() const{return kCapacity;}

private:
	enum { kCapacity=2 };
	P12DiagnosticWriterQueue():m_thread(&P12DiagnosticWriterQueue::run,this){}
	~P12DiagnosticWriterQueue(){
		{
			std::lock_guard<std::mutex> lock(m_mutex);m_stopping=true;
		}
		m_ready.notify_all();if(m_thread.joinable())m_thread.join();
		std::cout<<"[P6LinearWriterDrain] submitted="<<m_submitted.load()
			<<" completed="<<m_completed.load()<<" failed="<<m_failed.load()
			<<" maxQueueDepth="<<m_maxDepth<<" capacity="<<kCapacity<<std::endl;
	}
	static double elapsedMs(const std::chrono::steady_clock::time_point& begin){
		return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
	}
	static bool writeBytes(const std::string& path,const char* bytes,std::size_t count,double& ioMs){
		const auto begin=std::chrono::steady_clock::now();
		std::ofstream file(path.c_str(),std::ios::binary|std::ios::trunc);
		if(file.is_open()&&count)file.write(bytes,static_cast<std::streamsize>(count));
		file.flush();const bool ok=file.good();file.close();ioMs=elapsedMs(begin);return ok;
	}
	static bool serializePfm(PfmFile& pfm,const std::string& path,double& serializeMs,
			std::size_t& byteCount,double& ioMs){
		const auto begin=std::chrono::steady_clock::now();
		std::ostringstream stream(std::ios::out|std::ios::binary);
		const bool encoded=pfm.write(stream,Filename::from_os_specific(path));
		const std::string payload=encoded?stream.str():std::string();
		serializeMs=elapsedMs(begin);byteCount=payload.size();
		return encoded&&writeBytes(path,payload.data(),payload.size(),ioMs);
	}
	void run(){
		for(;;){
			P12DiagnosticWriteJob job;
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_ready.wait(lock,[this]{return m_stopping||!m_jobs.empty();});
				if(m_jobs.empty()){if(m_stopping)return;continue;}
				job=std::move(m_jobs.front());m_jobs.pop_front();
			}
			const auto workerBegin=std::chrono::steady_clock::now();
			const double queueWaitMs=std::chrono::duration<double,std::milli>(workerBegin-job.enqueued).count();
			double statsSerializeMs=0.0,statsFileIoMs=0.0,viewportCpuCopyMs=0.0;
			double pfmSerializeMs=0.0,pfmFileIoMs=0.0,formatConvertMs=0.0,pngSerializeMs=0.0,pngFileIoMs=0.0;
			std::size_t statsBytes=0,pfmBytes=0,pngBytes=0;
			bool statsOk=!job.statsRequired,pfmOk=false,pngOk=false;
			if(job.stats){
				statsOk=serializePfm(*job.stats,job.captureBase+"_stats.pfm",statsSerializeMs,statsBytes,statsFileIoMs);
			}
			if(job.linear){
				PfmFile viewport;
				PfmFile* output=job.linear.get();
				if(job.linear->get_x_size()!=job.width||job.linear->get_y_size()!=job.height){
					const auto copyBegin=std::chrono::steady_clock::now();
					viewport.clear(job.width,job.height,3);
					for(int y=0;y<job.height;++y)for(int x=0;x<job.width;++x)
						viewport.set_point3(x,y,job.linear->get_point3(x,y+job.linear->get_y_size()-job.height));
					viewportCpuCopyMs=elapsedMs(copyBegin);output=&viewport;
				}
				const std::string pfmPath=job.captureBase+".pfm";
				pfmOk=serializePfm(*output,pfmPath,pfmSerializeMs,pfmBytes,pfmFileIoMs);
				if(pfmOk){
					std::ostringstream line;line<<"[P6LinearCapture] sourceSeq="<<job.sourceSeq
						<<" size="<<job.linear->get_x_size()<<"x"<<job.linear->get_y_size()
						<<" validViewport="<<job.width<<"x"<<job.height<<" stage=pre_display"
						<<" domain="<<job.domain<<" unit="<<job.unit
						<<" common_scaled_linear="<<job.commonScaledLinear
						<<" physicalRadiance="<<job.physicalRadiance
						<<" readbackRoute="<<job.readbackRoute<<" asyncCpuWriter=1 file="<<pfmPath;
					std::cout<<line.str()<<std::endl;
				}else std::cerr<<"[P6LinearCapture][ERROR] sourceSeq="<<job.sourceSeq
					<<" reason=pfm_write_failed file="<<pfmPath<<std::endl;
			}
			try{
				const auto formatBegin=std::chrono::steady_clock::now();
				cv::Mat rgb(job.height,job.width,CV_8UC3,job.rgb.data()),bgr;
				cv::cvtColor(rgb,bgr,cv::COLOR_RGB2BGR);cv::flip(bgr,bgr,0);
				formatConvertMs=elapsedMs(formatBegin);
				const auto serializeBegin=std::chrono::steady_clock::now();
				std::vector<unsigned char> encoded;const bool encodedOk=cv::imencode(".png",bgr,encoded);
				pngSerializeMs=elapsedMs(serializeBegin);pngBytes=encoded.size();
				const std::string pngPath=job.captureBase+"_rgb8.png";
				pngOk=encodedOk&&writeBytes(pngPath,reinterpret_cast<const char*>(encoded.data()),encoded.size(),pngFileIoMs);
				if(!pngOk)std::cerr<<"[P6LinearCapture][ERROR] sourceSeq="<<job.sourceSeq
					<<" reason=png_write_failed file="<<pngPath<<std::endl;
			}catch(const cv::Exception& e){
				std::cerr<<"[P6LinearCapture][ERROR] sourceSeq="<<job.sourceSeq
					<<" reason=png_encode_exception message="<<e.what()<<std::endl;
			}
			const bool success=statsOk&&pfmOk&&pngOk;
			if(success)++m_completed;else ++m_failed;
			std::ostringstream perf;perf<<std::fixed<<std::setprecision(3)
				<<"[P6LinearWriterPerf] sourceSeq="<<job.sourceSeq
				<<" mode=bounded_async_cpu_writer queueWaitMs="<<queueWaitMs
				<<" workerTotalMs="<<elapsedMs(workerBegin)
				<<" viewportCpuCopyMs="<<viewportCpuCopyMs
				<<" statsSerializeMs="<<statsSerializeMs<<" statsFileIoMs="<<statsFileIoMs<<" statsBytes="<<statsBytes
				<<" pfmSerializeMs="<<pfmSerializeMs<<" pfmFileIoMs="<<pfmFileIoMs<<" pfmBytes="<<pfmBytes
				<<" formatConvertMs="<<formatConvertMs<<" pngSerializeMs="<<pngSerializeMs
				<<" pngFileIoMs="<<pngFileIoMs<<" pngBytes="<<pngBytes
				<<" statsWriteOk="<<(statsOk?1:0)<<" pfmWriteOk="<<(pfmOk?1:0)
				<<" pngWriteOk="<<(pngOk?1:0)<<" success="<<(success?1:0);
			std::cout<<perf.str()<<std::endl;
		}
	}
	std::mutex m_mutex;
	std::condition_variable m_ready;
	std::deque<P12DiagnosticWriteJob> m_jobs;
	std::thread m_thread;
	bool m_stopping=false;
	std::atomic<std::uint64_t> m_submitted{0},m_completed{0},m_failed{0};
	std::size_t m_maxDepth=0;
};
