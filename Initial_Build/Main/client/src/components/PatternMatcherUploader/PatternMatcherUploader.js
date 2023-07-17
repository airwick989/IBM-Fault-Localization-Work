import { useState } from "react";
import './styles.css';
import axios from 'axios';
//import { useNavigate } from "react-router-dom";

function PatternMatcherUploader() {

    //the reason the initial state is '[]' instead of 'null' is because its an array of files, not just one
    const [files, setFiles] = useState([]);


    const onFileInputChange = (e) => {
        setFiles(e.target.files);
    }


    const submitHandler = (e) => {
        e.preventDefault();

        const data = new FormData();
        for(let i = 0; i < files.length; i++){
            data.append('file', files[i]);
            //console.log(files[i])
        }

        alert('Pattern Matcher started')

        axios.post('http://localhost:5000/startPatternMatcher', data)
            .then( (e) => {
                if(e.data === "ok"){
                    console.log('nice')
                    //navigate("/loading");
                }
                else if(e.data === "FileNameError"){
                    console.error('FileNameError: Please ensure you upload only CSV files and a Jar file with the correct names.')
                    alert('FileNameError: Please ensure you upload only CSV files and a Jar file with the correct names.')
                }
            })
            .catch( (e) => {
                console.error('Error: ', e)
                alert('Error: ' + e)
            })
    }


    return (
        <form method="post" action="#" id="#" onSubmit={submitHandler} encType="multipart/form-data">

            <div class="max-w-md" style={{textAlign: "center"}}>
                <p class="py-6">After analyzing the localization results and prioritizing which source files the lock contentions exist in, 
                please upload those source files below.</p>
                
                <label class="label">
                    <span class="label-text" style={{marginTop: 10}}>Insert Source Files Here <b style={{color:'orange'}}>(Java source files only)</b></span>
                </label>
                <input type="file" 
                    class="file-input file-input-bordered file-input-accent w-full max-w-xs" 
                    required
                    multiple
                    onChange={onFileInputChange}
                    accept=".java"
                />
                
                <button className='btn btn-primary' type='submit' style={{marginTop: 50}}>Send to Classifier</button>
            </div>

        </form>
    )

}
export default PatternMatcherUploader