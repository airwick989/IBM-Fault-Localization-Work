// eslint-disable-next-line
import { Link } from 'react-router-dom';
// eslint-disable-next-line
import React, { useState } from "react";
import { FileUploader } from './components/FileUploader';

const Home = () => {

    // eslint-disable-next-line
    const handleSubmit = (e) => {
        e.preventDefault();
    }


    return ( 
        <div className="Home">

            <div class="hero min-h-screen bg-base-200">
            <div class="hero-content text-center">

                <FileUploader/>

            </div>
            </div>

        </div>
    );
}

export default Home;