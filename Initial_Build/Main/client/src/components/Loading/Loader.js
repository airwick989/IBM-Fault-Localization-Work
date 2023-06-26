import {Circles} from "react-loader-spinner";
const LoaderComp = () => {
 
    return (
        <Circles
            height="150"
            width="150"
            color="#36ccbe"
            ariaLabel="circles-loading"
            wrapperStyle={{}}
            wrapperClass=""
            visible={true}
        />
 
    );
}
export default LoaderComp;