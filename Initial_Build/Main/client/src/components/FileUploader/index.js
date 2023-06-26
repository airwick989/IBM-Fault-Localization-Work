import { useState } from "react";
import { useNavigate } from "react-router-dom";
import axios from 'axios';

const default_start_time = "15"
const default_recording_length = "20"

// eslint-disable-next-line
export const FileUploader = ({}) => {

    const navigate = useNavigate();

    //the reason the initial state is '[]' instead of 'null' is because its an array of files, not just one
    const [files, setFiles] = useState([]);
    const [args, setArgs] = useState("");
    const [start_time, setStartTime] = useState(default_start_time);
    const [recording_length, setRecordingLength] = useState(default_recording_length);

    //e is the parameter, in this case, e is an event
    const onFileInputChange = (e) => {
        setFiles(e.target.files);
    }

    const onStartTimeChange = (e) => {
        if(/^0\d*$/.test(e.target.value)){
            setStartTime("0")
        }
        else if(/^\d*$/.test(e.target.value)){
            setStartTime(e.target.value)
        }
    }

    const onRecordingLengthChange = (e) => {
        if(/^0\d*$/.test(e.target.value)){
            setRecordingLength("0")
        }
        else if(/^\d*$/.test(e.target.value)){
            setRecordingLength(e.target.value)
        }
    }

    const submitHandler = (e) => {
        e.preventDefault();
        
        if(files.length !== 4){
            alert("Please upload 3 CSV files and 1 Jar file according to the highlighted instructions!")
        }
        else{

            const data = new FormData();
            for(let i = 0; i < files.length; i++){
                data.append('file', files[i]);
                //console.log(files[i])
            }

            data.append('args', args)
            
            if(start_time === ""){
                data.append('start_time', parseInt(default_start_time))
            }
            else{
                data.append('start_time', parseInt(start_time))
            }

            if(recording_length === ""){
                data.append('recording_length', parseInt(default_recording_length))
            }
            else{
                data.append('recording_length', parseInt(recording_length))
            }

            // // print data to console
            // for (var pair of data.entries()) {
            //     console.log(pair[0]+ ', ' + pair[1]); 
            // }

            axios.post('http://localhost:5000/upload', data)
                .then( (e) => {
                    if(e.data === "ok"){
                        console.log('Files Uploaded Successfully')
                        //alert('Files Uploaded Successfully')
                        navigate("/loading");
                        //window.location.reload();
                    }
                    else if(e.data === "FileNameError"){
                        console.error('FileNameError: Please ensure you upload only CSV files and a Jar file with the correct names.')
                        alert('FileNameError: Please ensure you upload only CSV files and a Jar file with the correct names.')
                    }
                    else if(e.data === "FileCountError"){
                        console.error('FileCountError: Please enter EXACTLY 3 CSV files according to the specified naming conventions and EXACTLY 1 Jar file.')
                        alert('FileCountError: Please enter EXACTLY 3 CSV files according to the specified naming conventions and EXACTLY 1 Jar file.')
                    }
                    else{
                        console.error('Error: ', e)
                        alert('Error: ' + e)
                    }
                })
                .catch( (e) => {
                    console.error('Error: ', e)
                    alert('Error: ' + e)
                })

        }

    }

//     const submitHandler = (e) => {
//         e.preventDefault();
//         const formData = new FormData();
//         for(let i = 0; i < files.length; i++){
//             formData.append('file' + i, files[i]);
//         }

//         return postFile('/upload', formData).then((res) => res);
//     }

//     /**
//  * 
//  * @param {string} endpoint 
//  * @param {FormData} formData 
//  * @returns 
//  */
//     let postFile = (endpoint, formData) => {
//     const customHeader = {
//       headers: {
//         // Authorization: `Bearer ${getLocalStorageToken()}`,
//         "Content-Type": 'multipart/form-data',
//       },
//     };
  
//     let url = `http://localhost:5000${endpoint}`;
//         return axios
//         .post(url, formData, customHeader)
//         .then((res) => ({
//             status: res.status,
//             data: res.data,
//             error: null,
//         }))
//         .catch((err) => {
//             return {
//             status: err.response ? err.response.status : 0,
//             data: {},
//             error: err.message,
//             };
//         });
//     };
  

    return (
        <form method="post" action="#" id="#" onSubmit={submitHandler} encType="multipart/form-data">

            <div class="max-w-md">
                <h1 class="text-5xl font-bold">Enter Your Files Here</h1>
                <p class="py-6">Please provide the combined JLM, perf, and test CSV files along with the 
                Jar file to the dropbox. These files will be sent to the Classifier and Localizer
                in order to retrieve the classification and Localization data.</p>
                <p style={{color:'orange', fontWeight:'bold'}}>Please ensure the files are named correctly!</p>
                <p>Your JLM CSV should be titled '<b style={{color:'orange'}}>jlm.csv</b>', your perf CSV 
                should be titled '<b style={{color:'orange'}}>perf.csv</b>', and your test CSV should be titled 
                '<b style={{color:'orange'}}>test.csv</b>'</p>
                <p>Your Jar file can have any filename, so long as it is a '<b style={{color:'orange'}}>.jar</b>' file</p>
                
                <label class="label">
                    <span class="label-text" style={{marginTop: 50}}>Insert All Files Here</span>
                </label>
                <input type="file" 
                    class="file-input file-input-bordered file-input-accent w-full max-w-xs" 
                    required
                    multiple
                    onChange={onFileInputChange}
                    accept=".csv, .jar"
                />

                <label class="label">
                    <span class="label-text" style={{marginTop: 50}}>Insert Any Arguments to Pass to the Java Program Here<br/>(Separated by Spaces)</span>
                </label>
                <input type="text" 
                    class="input input-bordered input-accent w-full max-w-xs" 
                    value={args}
                    placeholder="arg0 arg1 arg2"
                    onChange={(e)=>setArgs(e.target.value)}
                />

                <label class="label">
                    <span class="label-text" style={{marginTop: 50}}>Insert How Long You Want Your Application to Run Before Localization Begins. The Default Value is {default_start_time}.<br/>(Integer Values Only, Time is in Seconds)</span>
                </label>
                <input type="text" 
                    class="input input-bordered input-accent w-full max-w-xs" 
                    value={start_time}
                    placeholder={default_start_time}
                    onChange={onStartTimeChange}
                />

                <label class="label">
                    <span class="label-text" style={{marginTop: 50}}>Insert How Long You Want the Localization to Run for. The Default Value is {default_recording_length}.<br/>(Integer Values Only, Time is in Seconds)</span>
                </label>
                <input type="text" 
                    class="input input-bordered input-accent w-full max-w-xs" 
                    value={recording_length}
                    placeholder={default_recording_length}
                    onChange={onRecordingLengthChange}
                />
                
                <button className='btn btn-primary' type='submit' style={{marginTop: 50}}>Send to Classifier</button>
            </div>

        </form>
    )
};