import { useState } from "react";
import axios from 'axios';

// eslint-disable-next-line
export const FileUploader = ({}) => {

    //the reason the initial state is '[]' instead of 'null' is because its an array of files, not just one
    const [files, setFiles] = useState([]);

    //e is the parameter, in this case, e is an event
    const onInputChange = (e) => {
        setFiles(e.target.files);
    }

    const submitHandler = (e) => {
        e.preventDefault();
        
        if(files.length !== 3){
            alert("Please upload 3 CSV files according to the highlighted instructions!")
        }
        else{

            const data = new FormData();
            for(let i = 0; i < files.length; i++){
                data.append('file', files[i]);
                //console.log(files[i])
            }

            axios.post('http://localhost:5000/upload', data)
                .then( (e) => {
                    console.log('Success')
                })
                .catch( (e) => {
                    console.error('Error', e)
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
                <p class="py-6">Please provide the combined JLM, perf, and test CSV files to the dropbox. 
                These files will be sent to the Classifier in order to retrieve the classification data.</p>
                <p style={{color:'orange', fontWeight:'bold'}}>Please ensure the files are named correctly!</p>
                <p>Your combined JLM CSV should be titled '<b style={{color:'orange'}}>jlm.csv</b>', your combined perf CSV 
                should be titled '<b style={{color:'orange'}}>perf.csv</b>', and your combined test CSV should be titled 
                '<b style={{color:'orange'}}>test.csv</b>'</p>
                
                <label class="label">
                    <span class="label-text" style={{marginTop: 50}}>Insert All CSV Files Here</span>
                </label>
                <input type="file" 
                    class="file-input file-input-bordered file-input-accent w-full max-w-xs" 
                    required
                    multiple
                    onChange={onInputChange}
                    accept=".csv"
                />
                
                <button className='btn btn-primary' type='submit' style={{marginTop: 50}}>Send to Classifier</button>
            </div>

        </form>
    )
};